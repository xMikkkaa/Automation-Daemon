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