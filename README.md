# Hermes URLs for Unreal Engine

Hermes URLs is a plugin for Unreal Engine (and a small tool, `hermes_urls`) that out of the box allows you to copy URLs to arbitrary assets in your project and share them with your team e.g. through Slack. Those links will then directly open the Unreal Editor to the linked asset.

In addition, Hermes provides easy-to-use APIs to register your own endpoints, so that you can create other direct deep links into the editor. E.g. you could create links that run automatic tests, link directly to a settings page, or whatever else strikes your fancy!

## Setup

Hermes requires you to [install Rust](https://www.rust-lang.org/tools/install) to build the associated `hermes_urls` tool.

1. Clone this repository into your project's `Plugins` folder
1. Build `hermes_urls`:
    1. Navigating a command prompt to `Hermes/HermesCore/Source/HermesURLHandler`
    2. Run `cargo build --release`.
1. Build & start your editor - the URL is automatically registered when the editor first starts

**Note**: You can check in the resulting executable (`Hermes/HermesCore/Source/HermesURLHandler/target/release/hermes_urls.exe`) so that other people on your team do not need to build it. It only needs to be rebuilt if you modify the Rust source or update Hermes URLs.

By default, Hermes will register URIs that match the project name of your project. If you need more control over the scheme used by these URIs, look at the `HermesUriSchemeBranchReplacement` plugin which lives next to `HermesCore` (and is not enabled by default). Either you can use it as-is simply by enabling it and configuring it, or you can use it as a starting point for developing your own `IHermesUriSchemeProvider`.

## Using

Once you've set up Hermes, you should be able to right click any asset in the content browser and see a new "*Copy URL*" option:

[<img src="README_contentbrowser.png?raw=true" width=50%>](README_contentbrowser.png?raw=true)

Similarly, when you've opened any asset in the asset editor, you should see a new "*Copy URL*" option in the "Asset" option from the menu bar:

[<img src="README_asseteditor.png?raw=true" width=50%>](README_asseteditor.png?raw=true)

## License

Hermes URLs is licensed under either of

 * Apache License, Version 2.0
   ([LICENSE-APACHE](LICENSE-APACHE) or http://www.apache.org/licenses/LICENSE-2.0)
 * MIT license
   ([LICENSE-MIT](LICENSE-MIT) or http://opensource.org/licenses/MIT)

at your option.

## Contribution

Unless you explicitly state otherwise, any contribution intentionally submitted
for inclusion in the work by you, as defined in the Apache-2.0 license, shall be
dual licensed as above, without any additional terms or conditions.