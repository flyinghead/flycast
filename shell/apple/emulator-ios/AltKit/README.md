# AltKit

AltKit allows apps to communicate with AltServers on the same WiFi network and enable features such as JIT compilation.

## Installation

To use AltKit in your app, add the following to your `Package.swift` file's dependencies:

```
.package(url: "https://github.com/rileytestut/AltKit.git", .upToNextMajor(from: "0.0.1")),
```

Next, add the AltKit package as a dependency for your target:

```
.product(name: "AltKit", package: "AltKit"),
```

Finally, right-click on your app's `Info.plist`, select "Open As > Source Code", then add the following entries:

```
<key>NSBonjourServices</key>
<array>
    <string>_altserver._tcp</string>
</array>
<key>NSLocalNetworkUsageDescription</key>
<string>[Your app] uses the local network to find and communicate with AltServer.</string>
```

## Usage

### Swift
```
import AltKit

ServerManager.shared.startDiscovering()

ServerManager.shared.autoconnect { result in
    switch result
    {
    case .failure(let error): print("Could not auto-connect to server.", error)
    case .success(let connection):
        connection.enableUnsignedCodeExecution { result in
            switch result
            {
            case .failure(let error): print("Could not enable JIT compilation.", error)
            case .success: 
                print("Successfully enabled JIT compilation!")
                ServerManager.shared.stopDiscovering()
            }
            
            connection.disconnect()
        }
    }
}
```

### Objective-C
```
@import AltKit;

[[ALTServerManager sharedManager] startDiscovering];

[[ALTServerManager sharedManager] autoconnectWithCompletionHandler:^(ALTServerConnection *connection, NSError *error) {
    if (error)
    {
        return NSLog(@"Could not auto-connect to server. %@", error);
    }
    
    [connection enableUnsignedCodeExecutionWithCompletionHandler:^(BOOL success, NSError *error) {
        if (success)
        {
            NSLog(@"Successfully enabled JIT compilation!");
            [[ALTServerManager sharedManager] stopDiscovering];
        }
        else
        {
            NSLog(@"Could not enable JIT compilation. %@", error);
        }
        
        [connection disconnect];
    }];
}];
```
