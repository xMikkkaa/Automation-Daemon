/*
 * Copyright 2026 xMikkkaa
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

use std::fs;
use std::path::PathBuf;
use std::sync::Mutex;
use std::sync::atomic::{AtomicBool, Ordering};
use std::os::unix::fs::PermissionsExt;

static IS_IDLE_CHARGING_ACTIVE: AtomicBool = AtomicBool::new(false);
static ORIGINAL_CURRENTS: Mutex<Option<Vec<(String, String)>>> = Mutex::new(None);


struct ChargeSwitch {
    path: &'static str,
    enable: &'static str,
    disable: &'static str,
    use_max: bool,
}

const SWITCHES: &[ChargeSwitch] = &[
    ChargeSwitch { path: "/sys/class/power_supply/battery/constant_charge_current_max", enable: "3000000", disable: "100000", use_max: true },
];



fn read_sysfs_backup(path_str: &str) -> Option<String> {
    if let Ok(content) = fs::read_to_string(crate::config::AUTD_SYSFS_BACKUP_PATH) {
        for line in content.lines() {
            let mut parts = line.split('|');
            if let (Some(p), Some(v)) = (parts.next(), parts.next()) {
                if p == path_str {
                    return Some(v.to_string());
                }
            }
        }
    }
    None
}

fn is_valid_max(val_str: &str) -> bool {
    let trim_val = val_str.trim();
    if trim_val.is_empty() || trim_val == "0" {
        return false;
    }
    if let Ok(num) = trim_val.parse::<i64>() {
        if num < 400 || (num > 10000 && num < 400000) {
            return false;
        }
    } else {
        return false;
    }
    true
}

fn get_safe_fallback(path: &PathBuf, default_enable: &str) -> String {
    if let Some(backup) = read_sysfs_backup(&path.to_string_lossy()) {
        if is_valid_max(&backup) {
            return backup;
        }
    }

    let max_path = path.with_file_name(format!("{}_max", path.file_name().unwrap_or_default().to_string_lossy()));
    if max_path.exists() {
        if let Ok(val) = fs::read_to_string(&max_path) {
            if is_valid_max(&val) {
                return val.trim().to_string();
            }
        }
    }

    let alternative_paths = [
        "/sys/class/power_supply/battery/constant_charge_current_max",
        "/sys/class/power_supply/main/current_max",
        "/sys/class/power_supply/usb/current_max"
    ];
    for alt in alternative_paths.iter() {
        if let Ok(val) = fs::read_to_string(alt) {
            if is_valid_max(&val) {
                return val.trim().to_string();
            }
        }
    }

    default_enable.to_string()
}

fn get_active_switches() -> Vec<&'static ChargeSwitch> {
    let mut active = Vec::new();
    let mut found_limit = false;
    
    for sw in SWITCHES.iter() {
        if PathBuf::from(sw.path).exists() {
            if sw.use_max {
                found_limit = true;
                active.push(sw);
            } else if !found_limit {
                active.push(sw);
                break;
            }
        }
    }
    active
}



fn write_sysfs(path: &PathBuf, val: &str) {
    if let Ok(current_val) = fs::read_to_string(path) {
        if current_val.trim() == val.trim() {
            return;
        }
    }

    if let Ok(metadata) = fs::metadata(path) {
        let mut perms = metadata.permissions();
        perms.set_mode(0o644);
        let _ = fs::set_permissions(path, perms);
    }
    let _ = fs::write(path, format!("{}\n", val));
}

pub fn init_backup_once() {
    if let Ok(content) = fs::read_to_string(crate::config::AUTD_SYSFS_BACKUP_PATH) {
        if content.lines().count() > 0 {
            return;
        }
    }

    let mut out = String::new();
    for sw in SWITCHES.iter() {
        if !sw.use_max { continue; }
        let path = PathBuf::from(sw.path);
        let fallback = get_safe_fallback(&path, sw.enable);
        out.push_str(&format!("{}|{}\n", sw.path, fallback));
    }
    
    if !out.is_empty() {
        let _ = fs::write(crate::config::AUTD_SYSFS_BACKUP_PATH, out);
    }
}

pub fn enable_idle_charging() {
    let active_switches = get_active_switches();
    if active_switches.is_empty() {
        return;
    }

    let mut originals = Vec::new();
    let is_first_time = !IS_IDLE_CHARGING_ACTIVE.load(Ordering::Relaxed);

    for sw in &active_switches {
        let path = PathBuf::from(sw.path);
        
        if is_first_time {
            let mut original_val = String::new();
            if sw.use_max {
                original_val = get_safe_fallback(&path, sw.enable);
            } else {
                if let Ok(current_val) = fs::read_to_string(&path) {
                    original_val = current_val.trim().to_string();
                }
                if original_val.is_empty() || original_val == sw.disable {
                    original_val = sw.enable.to_string();
                }
            }
            
            originals.push((sw.path.to_string(), original_val));
        }
        
        write_sysfs(&path, sw.disable);
    }

    if is_first_time {
        if let Ok(mut store) = ORIGINAL_CURRENTS.lock() {
            if store.is_none() && !originals.is_empty() {
                *store = Some(originals);
            }
        }
        IS_IDLE_CHARGING_ACTIVE.store(true, Ordering::Relaxed);
    }
}

pub fn disable_idle_charging() {
    if !IS_IDLE_CHARGING_ACTIVE.load(Ordering::Relaxed) {
        return;
    }
    
    if let Ok(mut store) = ORIGINAL_CURRENTS.lock() {
        if let Some(originals) = store.take() {
            for (path_str, val) in originals {
                let path = PathBuf::from(&path_str);
                write_sysfs(&path, &val);
            }
        } else {
            let active_switches = get_active_switches();
            for sw in active_switches {
                let path = PathBuf::from(sw.path);
                if sw.use_max {
                    write_sysfs(&path, &get_safe_fallback(&path, sw.enable));
                } else {
                    write_sysfs(&path, sw.enable);
                }
            }
        }
    }
    
    IS_IDLE_CHARGING_ACTIVE.store(false, Ordering::Relaxed);
}



pub fn reset_charging_states() {
    IS_IDLE_CHARGING_ACTIVE.store(false, Ordering::Relaxed);
    if let Ok(mut store) = ORIGINAL_CURRENTS.lock() { *store = None; }
}

pub fn get_battery_level() -> i32 {
    if let Ok(bytes) = fs::read("/sys/class/power_supply/battery/capacity") {
        let mut val = 0;
        let mut has_digit = false;
        
        for &b in bytes.iter() {
            if b.is_ascii_digit() {
                val = val * 10 + (b - b'0') as i32;
                has_digit = true;
            } else if has_digit {
                break;
            }
        }
        
        if has_digit {
            return val.clamp(0, 100);
        }
    }
    100
}

pub fn is_android_powersave() -> bool {
    if let Ok(bytes) = fs::read(crate::config::AUTD_PS_STATE_PATH) {
        return bytes.first() == Some(&b'1');
    }
    false
}