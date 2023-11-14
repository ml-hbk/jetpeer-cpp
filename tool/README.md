# Examples and Tools for the C++ jet peer

Here you can find some example programs for doing specific tasks using jet. Each program will use a jet peer to work on jet.
All programs rely on a running jet damon to connect to.

## jetcat

Connects to a jet daemon and fetches all states and methods.

## jetstate

Connects to a jet daemon and creates a jet state that can be fetched and set [(see jetset)](jetset) by other jet peers.

## jetset

Connects to a jet daemon, sends a request to set a state and waits for the response.

## jetmethod

Connects to a jet daemon and create a jet method that can be executed [(see jetexec)](jetexec) by other jet peers.  

## jetexec

Connects to a jet daemon, calls a jet method and waits for the response


# How to use them


## Creating and Fetching a state

In this example jetstate will act as a state owner and jetcat listens what is happening on jet.
We expect the jet daemon to be running on the local machine.

First simply start `jetcat` in a console without parameters. In this case the jet peer connects to the jet daemon on the local machine.
Nohing should happen since there are no states or methods registered in the moment. The program just waits.

Now take another console and start `jetstate` as follows:

```
jetstate aState int 42
```


After doing this, `jetcat` shows the following output:

```
state 'aState' added
42
```

This means that a new state called `aState` was created by `jetstate`.

After killing `jetstate` you'll see the following output from jetcat:

```
state 'aState' removed
42
```

As you can see the `aState` disappeared with `jetstate` which owned it.

## Creating, Fetching and Changing a State

Here we extend the previous example by changing an existing state by another peer.
Again start `jetcat` and `jetstate` as mentioned above.

```
jetcat
jetstate aState int 42
```

now open a third console and execute `jetset` to manipulate `aState`:

```
jetset 127.0.0.1 11122   astate int 12
```

The first parameter tells to connect to the jet daemon on the local machine. The seconds one tell the TCP port to use.
The jet daemon by default listens to port 11122. The third parameter is the state to maipulate followed by the data type and value.

Since an existing state is changed, `jetcat` will show the following output:
```
state 'astate' changed
12
```

`jetstate` which is the state owner shows the folloing output:
```
set state to 12
```
It just received the request to change the value of the stated. The requested value was validated, 
set and the new state value was notified back to the jetdaemon. The jet daemon recognized that another peer,
`jetcat`, is interested in any changed state and send a notification.

Please try `jet` with a wrong jet damon address or try to set a non-existing state. In both cases, you will get an error.

## Serving and Executing Methods

`jetmethod` registers a jet function that can be called by any jet peer. When the calling a jet method, the registered callback function is being executed.
In the case of `jetmethod` the requested value is simple mirrored and returned as the result of the method. The method is served until `jetmethod` is terminated.

`jetexec` on the other hand is used to call any jet method.

```
jetexec 127.0.0.1 11122 theMethod 20
```

The first and second paramters tell address and port of the jet daemon to connect to. `theMethod` is the path of the method.
the last parameter is the requested value.

using an address/port where not jet damon is listening or provifing a path that is not being served will result in an error being returned.


