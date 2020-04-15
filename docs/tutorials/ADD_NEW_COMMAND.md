# Add a new command

The command types can be found in the [Protocol specification](/docs/PROTOCOL.md#command_types) document. To add a new 
command, expand the list in the documentation itself with a new type.

The list of commands can be found in [cs_CommandTypes.h](/source/include/protocol/cs_CommandTypes.h). They are numbered

```
enum CommandHandlerTypes {
	...
	CTRL_CMD_YOUR_COMMAND = N,
}
```

## Data types

There is a large list of types in [cs_Types.h](/source/include/common/cs_Types.h). There are three classes:

1. State types
2. Event types
3. Command types.

For example, when adding only a new `State type` you don't necessarily have to add a different type of payload to the 
protocol. There is a 
[state_set_packet](/docs/PROTOCOL.md#state_set_packet) and a 
[state_get_packet](/docs/PROTOCOL.md#state_get_packet) with a variable size payload.

If you do not want to have it stored as a state variable in `cs_State`, or if there are other reasons to define a 
custom command, you have to add it to `CS_TYPE`. 

```
enum class CS_TYPE: uint16_t {
	...
	CMD_YOUR_COMMAND = N,
}
```

This is a different index then in `CommandHandlerTypes` which is only used to represent the control opcodes for the 
protocol. The `CS_TYPE` can be seen as a something that allows you to:

* Write a `typedef` that indicates the format of this (command) type.
* Have the `TypeSize(CS_TYPE)` function return the size of this type.
* Have the `TypeName(CS_TYPE)` function return a readable type string for debugging purposes.
* Have a `hasMultipleIds(CS_TYPE)` function to allow storage of multiple versions of this type.
* Have a `removeOnFactoryReset(CS_TYPE)` function to indicate if persistence should go beyond a factory reset.
* Have a `getUserAcccessLevelSet(CS_TYPE)` function to indicate GET access level for this type.
* Have a `setUserAcccessLevelSet(CS_TYPE)` function to indicate SET access level for this type.

There are also further options that allow you to specify in [csStateData.cpp](/source/src/storage/cs_StateData.cpp)
matters on (persistent) storage. You will have to indicate for non-state data (such as commands) that they are NOT
meant to be stored. This class allows you to:

* Have a `getDefault()` function that specifies the default value for a type (often the "factory default"). For a
command you'll often want to return `ERR_NOT_FOUND`.
* Have a `DefaultLocation` which is either `PersistenceMode::FLASH`, `RAM`, or `NEITHER_RAM_NOR_FLASH` (the latter in
case you do not want to store the type at all).

The reason that we have one over-arching type definition is that it makes it easy to send every possible type as an
event. Moreover, by specifying them as an `enum class` you will get compiler warnings if you forget to include them
in one of the large switch statements. Never add a `default` clause to those switch statements. Then we would not have
those compiler warnings anymore to prevent such mistakes.

The struct for the command you create can be specified in [cs_Packets.h](/source/include/protocol/cs_Packets.h). An
example is:

```
struct __attribute__((packed)) session_nonce_t {
	uint8_t data[SESSION_NONCE_LENGTH];
};
```

You see that the struct is `packed`, important! Moreover, it is of fixed size. Always specify a maximum size!

## Communication

To add the new command to the part of the code where the BLE messages arrive, or more specific, where the command
messages arrive, navigate to [cs_CommandHandler.cpp](/source/src/processing/cs_CommandHandler.cpp). There is the
`handleCommand` function that can be expanded.

You will have to do several things:

* Add `CTRL_CMD_YOUR_COMMAND` as valid option in the start.
* Add in the function `getRequiredAccessLevel` the access level, `ENCRYPTION_DISABLED`, `BASIC`, `MEMBER`, or `ADMIN`.
* Then handle the command itself, see next.

You can handle the command by adding a case statement:

```
void CommandHandler::handleCommand(...)
	...
	case CTRL_CMD_YOUR_COMMAND:
		return handleYourCommand(commandData, accessLevel, result);
```

Or you can immediately dispatch an event by:

```
void CommandHandler::handleCommand(...)
	...
	case CTRL_CMD_YOUR_COMMAND:
		return dispatchEventForCommand(CS_TYPE::CMD_YOUR_COMMAND, commandData, result);
```

Note the subtle change from the opcode `CTRL_CMD_YOUR_COMMAND` to the type `CS_TYPE::CMD_YOUR_COMMAND`.

When the command is send through as an event, the `dispatch()` function will synchronously update all event listeners.
However, those event listeners can have asynchronous calls. For example, writing something to flash and receiving a
"done" request after this operation has finished. The `result.returnCode`, `.data`, and `.dataSize` fields can 
therefore not always be written.

For this you will need to use notifications.
