## Architecture Overview

VideoCore's architecture is very simple and modular.  The basic concept of VideoCore's graph approach is to connect components that perform specific operations on the data to go from the supplied input format to the required output format.  There are three main component types used to accomplish this: *[Sources](#sources)*, *[Outputs](#outputs)*, and *[Transforms](#transforms)*.  It should be noted, though, that Transforms are a special class of Outputs that will pass their data to another output.


### Sources
*See [Sources](./Sources.md) for more information on sources*

Sources are subclasses of `videocore::ISource` and reside at the beginning of the transformation chain for a particular input type.  A source may, for example, vend sample buffers from an iPhone's camera or audio buffers from an application.


### Outputs
*See [Outputs](./Outputs.md) for more information on sources*

### Transforms
*See [Transforms](./Transforms.md)