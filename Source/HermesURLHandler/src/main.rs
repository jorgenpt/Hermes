// Copyright (c) Jørgen Tjernø <jorgen@tjer.no>. All rights reserved.
#![deny(clippy::all)]
// We use the console subsystem in debug builds, but use the Windows subsystem in release
// builds so we don't have to allocate a console and pop up a command line window.
// This needs to live in main.rs rather than windows.rs because it needs to be a crate-level
// attribute, and it doesn't affect the mac build at all, so it's innocuous to leave for
// both target_os.
#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]
#![cfg_attr(debug_assertions, windows_subsystem = "console")]

// Import os-specific code for Windows, if appropriate
#[cfg(target_os = "windows")]
pub mod windows;
#[cfg(target_os = "windows")]
pub use windows as os;

fn main() -> Result<(), Box<dyn std::error::Error>> {
    Ok(os::main()?)
}
