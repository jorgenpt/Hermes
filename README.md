# <img src="assets/hermes_icon.png?raw=true" width="24"> Hermes URLs for Unreal Engine

Hermes URLs is a plugin for Unreal Engine that out of the box allows you to copy URLs to arbitrary assets in your project and share them with your team e.g. through Slack. Those links will then directly open the Unreal Editor to the linked asset.

In addition, Hermes provides easy-to-use APIs to register your own endpoints, so that you can create other direct deep links into the editor. E.g. you could create links that run automatic tests, link directly to a settings page, or whatever else strikes your fancy!

Big thanks to Krista A. Leemhuis for the amazing icons!


## Setup

Hermes officially supports UE5 and is backwards compatible with UE4 version 4.27. Pull requests to support older versions are welcome.

1. Clone this repository into your project's `Plugins` folder
1. Start your editor - the URL is automatically registered when the editor first starts

By default, Hermes will register URIs that match the project name of your project. If you need more control over the scheme used by these URIs, you can use the `HermesBranchSupport` plugin which lives next to `HermesCore`, which lets you include the branch name in the URI scheme. You'll need to enable `HermesBranchSupport` in your .uproject, and then you can go to Edit > Preferences and find "Hermes URLs - Branch Support" under Plugins to configure it.

Hermes relies on [hermes_urls][hermes_urls] to register with the OS and dispatch URL requests. It's a small Rust project, and its binaries are checked in to this repository (in [HermesCore/Source/HermesURLHandler][hermesurlhandler]) for convenience's sake, but feel free to review the source and build your own if downloading EXE files from the internet puts you at (understandable) unease.


## Using

Once you've set up Hermes, you should be able to right click any asset in the content browser and see a new "*Copy URL that reveals asset*" option:

[<img src="README_contentbrowser.png?raw=true" width=50%>](README_contentbrowser.png?raw=true)

Similarly, when you've opened any asset in the asset editor, you should see a new "*Copy URL that opens asset*" option in the "Asset" option from the menu bar:

[<img src="README_asseteditor.png?raw=true" width=50%>](README_asseteditor.png?raw=true)


## Extending

Hermes is intended to be pretty customizable and extendible. Feel free to [reach out][email] if you have any questions, or send a pull request if you think your functionality should be a part of the core Hermes experience!

### Creating custom URLs with your own functionality

To see how to create your own handler for custom URLs you can look at [HermesContentEndpoint.cpp][hermescontentendpoint-cpp], which is the implementation of the asset links. The editor integration that lets you copy those links to the clipboard lives in [HermesContentEndpointEditorExtension.cpp][hermescontentendpointeditorextension-cpp].

You can create a similar module in your own project and depend on `HermesServer` from your module, and you should be good to go.

### Controlling what URL scheme / protocol your links have

If you want to have more control over the URL scheme / protocol than `Hermes` and `HermesBranchSupport` gives you, you can create your own `IHermesUriSchemeProvider`. It is a very small C++ interface that you register as a modular feature -- all you need to implement is a `TOptional<FString> GetPreferredScheme()` method. You can use [HermesBranchSupport.cpp][hermesbranchsupport-cpp] as a starting point for developing your own `IHermesUriSchemeProvider` to override the URI scheme used.


## License

[The icon](assets/hermes_icon.png) is copyright (c) 2022 [Jørgen P. Tjernø][email]. All Rights Reserved.

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

[hermes_urls]: https://github.com/jorgenpt/hermes_urls
[hermesurlhandler]: HermesCore/Source/HermesURLHandler
[hermescontentendpoint-cpp]: HermesCore/Source/HermesContentEndpoint/Private/HermesContentEndpoint.cpp
[hermescontentendpointeditorextension-cpp]: HermesCore/Source/HermesContentEndpoint/Private/HermesContentEndpointEditorExtension.cpp
[hermesbranchsupport-cpp]: HermesBranchSupport/Source/HermesBranchSupport/Private/HermesBranchSupport.cpp
[email]: mailto:jorgen@tjer.no
