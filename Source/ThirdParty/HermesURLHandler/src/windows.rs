use anyhow::{bail, Context, Result};
use log::{debug, error, info, trace, warn};
use mail_slot;
use simplelog::*;
use std::{
    fs::{File, OpenOptions},
    io,
    path::{Path, PathBuf},
};
use structopt::StructOpt;
use windows_bindings;
use winreg::{enums::*, RegKey};

// How many bytes do we let the log size grow to before we rotate it? We only keep one current and one old log.
const MAX_LOG_SIZE: u64 = 64 * 1024;

const DISPLAY_NAME: &str = "Hermes URL Handler";
const DESCRIPTION: &str = "Open links to UE4 assets or custom editor actions";

fn get_protocol_registry_key(protocol: &str) -> String {
    format!(r"SOFTWARE\Classes\{}", protocol)
}

fn get_configuration_registry_key(protocol: &str) -> String {
    format!(r"Software\bitSpatter\Hermes\{}", protocol)
}

/// Register associations with Windows to handle our protocol
fn register_protocol(protocol: &str, extra_args: Option<&str>) -> io::Result<()> {
    use std::env::current_exe;

    let exe_path = current_exe()?;
    let exe_path = exe_path.to_str().unwrap_or_default().to_owned();
    let icon_path = format!("\"{}\",0", exe_path);
    let open_command = if let Some(extra_args) = extra_args {
        format!("\"{}\" {} \"%1\"", exe_path, extra_args)
    } else {
        format!("\"{}\" \"%1\"", exe_path)
    };

    let hkcu = RegKey::predef(HKEY_CURRENT_USER);

    // Configure our ProgID to point to the right command
    let (progid_class, _) = hkcu.create_subkey(get_protocol_registry_key(protocol))?;
    progid_class.set_value("", &format!("URL:{} Protocol", protocol))?;

    // Indicates that this class defines a protocol handler
    progid_class.set_value("URL Protocol", &"")?;

    let (progid_class_defaulticon, _) = progid_class.create_subkey("DefaultIcon")?;
    progid_class_defaulticon.set_value("", &icon_path)?;

    let (progid_class_shell_open_command, _) = progid_class.create_subkey(r"shell\open\command")?;
    progid_class_shell_open_command.set_value("", &open_command)?;

    Ok(())
}

/// Register a new host & EXE pair
fn register_host(protocol: &str, host: &str, commandline: &Vec<String>) -> io::Result<()> {
    let _hkcu = RegKey::predef(HKEY_CURRENT_USER);

    Ok(())
}

/// Remove all the registry keys that we've set up
fn unregister_protocol(protocol: &str) {
    let hkcu = RegKey::predef(HKEY_CURRENT_USER);
    let _ = hkcu.delete_subkey_all(get_protocol_registry_key(protocol));

    // TODO #jorgen: Remove all host registrations
}

/// Remove a previous host registration
fn unregister_host(_protocol: &str, _hostname: &str) {
    // Find the current executable's name, so we can unregister it
    let _hkcu = RegKey::predef(HKEY_CURRENT_USER);
}

// This is the definition of our command line options
#[derive(Debug, StructOpt)]
#[structopt(
    name = DISPLAY_NAME,
    about = DESCRIPTION
)]
struct CommandOptions {
    /// Use verbose logging
    #[structopt(short, long)]
    verbose: bool,
    /// Use debug logging, even more verbose than --verbose
    #[structopt(long)]
    debug: bool,

    /// Choose the mode of operation
    #[structopt(subcommand)]
    mode: ExecutionMode,
}

#[derive(Debug, StructOpt)]
enum ExecutionMode {
    /// Dispatch the given URL to Unreal Engine (or launch it, if needed)
    Open {
        /// URL to open
        url: String,
    },

    /// Register this EXE as a URL protocol handler
    Register {
        /// The protocol this exe will be registered for
        protocol: String,
        /// Enable debug logging for this registration
        #[structopt(long)]
        register_with_debugging: bool,
    },

    /// Register a specific hostname (and, if needed, this EXE as an URL protocol handler)
    RegisterHost {
        /// The protocol this hostname will be registered for (this will implicity invoke `register` for the protocol if needed)
        protocol: String,
        /// The hostname to register a handler for
        hostname: String,
        /// The command line that will handle the registration if needed, where %1 is the placeholder for the URL
        commandline: Vec<String>,
    },

    /// Remove all registry entries for the URL protocol handler & hostname configuration
    Unregister {
        /// The protocol we will delete the registration for
        protocol: String,
    },

    /// Unregister a specific host name
    UnregisterHost {
        /// The protocol this hostname will be unregistered from
        protocol: String,
        /// The hostname whose handler we're unregistering
        hostname: String,
    },
}

fn get_exe_relative_path(filename: &str) -> io::Result<PathBuf> {
    let mut path = std::env::current_exe()?;
    path.set_file_name(filename);
    Ok(path)
}

fn rotate_and_open_log(log_path: &Path) -> Result<File, io::Error> {
    if let Ok(log_info) = std::fs::metadata(&log_path) {
        if log_info.len() > MAX_LOG_SIZE {
            if let Err(_) = std::fs::rename(&log_path, log_path.with_extension("log.old")) {
                if let Err(_) = std::fs::remove_file(log_path) {
                    return File::create(log_path);
                }
            }
        }
    }

    return OpenOptions::new().append(true).create(true).open(log_path);
}

fn init() -> Result<CommandOptions> {
    // First parse our command line options, so we can use it to configure the logging.
    let options = CommandOptions::from_args();
    let log_level = if options.debug {
        LevelFilter::Trace
    } else if options.verbose {
        LevelFilter::Debug
    } else {
        LevelFilter::Info
    };

    let mut loggers: Vec<Box<dyn SharedLogger>> = Vec::new();

    // Always log to hermes.log
    let log_path = get_exe_relative_path("hermes.log")?;
    loggers.push(WriteLogger::new(
        log_level,
        Config::default(),
        rotate_and_open_log(&log_path)?,
    ));

    // We only use the terminal logger in the debug build, since we don't allocate a console window otherwise.
    if cfg!(debug_assertions) {
        loggers.push(TermLogger::new(
            log_level,
            Config::default(),
            TerminalMode::Mixed,
        ));
    };

    CombinedLogger::init(loggers)?;
    trace!("command line options: {:?}", options);

    Ok(options)
}

pub fn main() -> Result<()> {
    let options = init()?;

    match options.mode {
        ExecutionMode::Register {
            protocol,
            register_with_debugging,
        } => {
            info!("registering handler for {}://", protocol);
            let extra_args = if register_with_debugging {
                Some("--debug")
            } else {
                None
            };

            register_protocol(&protocol, extra_args)
                .with_context(|| format!("Failed to register handler for {}://", protocol))?;
        }
        ExecutionMode::RegisterHost {
            protocol,
            hostname,
            commandline,
        } => register_host(&protocol, &hostname, &commandline)
            .with_context(|| format!("Failed to register host for {}://{}", protocol, hostname))?,
        ExecutionMode::Unregister { protocol } => {
            info!("unregistering handler for {}://", protocol);
            unregister_protocol(&protocol);
        }
        ExecutionMode::UnregisterHost { protocol, hostname } => {
            unregister_host(&protocol, &hostname);
        }
        ExecutionMode::Open { url } => {
            // TODO
        }
    }

    Ok(())
}
