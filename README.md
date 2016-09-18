# mprotocol-server
The `MProtocol server library` is a simple, characher-based protocol that can
be used to control remote (possibly embedded) applications via various
interfaces. The communication in this implementation is strictly ASCII, each
command is finished wih a `\n` or a `\r` character (can be both). Most
responses are single lines, the ones that are not always start with a `{`
character and end with `}`. The protocol only defines a small set of commands,
all the application's logic is implemented in the **node-tree**.

## `GET` command

The `GET` command is used to get information. It can be called with nodes and
properties also.

To get the **root node**'s content:
```
GET /
{
N TIM4
N GREEN
N ORANGE
N RED
N BLUE
}
```

To get another `Node` (that may also contain nodes and may have properties):
```
GET /RED
{
PW_BOOL Enabled=1
PW_UINT32 Pwm=35
}
```

To get a single property's value:
```
GET /RED.Pwm
PW_UINT32 Pwm=35
```

## `SET` command
The `SET` command is used to changing property values.

```
SET /RED.Pwm=100
E0:Ok
```

The second line is the response code (see `ProtocolResult_t`) and its
readable meaning.

## `CALL` command
The `CALL` command is used to invoke method's of `Node`s. Methods are not
like readable properties, they can obly be invoked.

(TODO: example)

## `OPEN` command
The `OPEN` command is used to subscribe to a `Node`'s changes (see
`Node::invalidateProperty()).

```
OPEN /RED
E0:Ok
SET /RED.Pwm=0
E0:Ok
CHG /RED.Enabled=0
```

Change reports (`CHG` line) are sent asynchronously when the application calls
`invalidateProperty` on a subscribed `Node`'s property.

## `CLOSE` command
The `CLOSE` command is used to clear subscription to a `Node`'s changes (see
also: `OPEN` command).

```
CLOSE /RED
E0:Ok
```

## `MAN` command
The `MAN` command is used to get the manual entry for `Node`'s and properties.

```
MAN /RED
MAN PWM controller for the Discovery board's LED's.
```

```
MAN /RED.Pwm
MAN PWM pulse width for this LED.
```
