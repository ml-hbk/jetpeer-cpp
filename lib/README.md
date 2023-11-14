Each jet peer connects to a jet daemon. The jet daemon manages states registered by peers. It routes requests from a requesting peer to the peer owning the state.

# Roles of a Peer

As already stated a peer needs to register states or methods before any other peer is able to make requests to such states or methods.
The peer that registers is called the owner (or agent). The peer that does requests is called the client.

## Owner Role

Jet peers register states or methods on the jet daemon. Owner notifies state changes to the jet daemon.

Owner gets requests to change states. If such an requests arrives, the owner validates the requets. If the request is invalid, a error response is send back. If the request is valid, it is acknowledged.
If the request results in a change of the state, the complete new state is being notified to the jet daemon.

## Client Role

Jet peer sends requests to change states and calls methods. 

Jet peers can also fetch for states. Such Fetches have a filter so that only the matching notifications will be delivered. When a fetch is established, the current values of all matching states are notified.


# Jet Daemon

The jet daemon knows all states and their values. It caches all values. 
The jet daemon knows all methods.
It knows the ownning peer of each state and method. If a jet client requests to change a state, the jet daemon routes the request to the owning peer.
The resulting response from the owning peer will be routed back to the requesting peer.


# Asynchronous and Synchronous Peer

The jet protocol is designed to work in an asynchronous way. Each request from a peer to the jet daemon carries an id. 
After processing the request, the jet daemon will send a response carrying the same id to the peer the request came from.
As a result, several requests might be in flight. The requesting peer aligns the matching responses.

## Asynchronous Peer `hbk::jet::PeerAsync`

Each request to another peer has a result callback function, which is being called on arrival of the response. 
As a result, many requests might run in parallel.

The constrcutor requires an `hbk::sys::EventLoop` that is used for receiving data from the jet daemon.

## Asynchronous Peer `hbk::jet::Peer`

This is an asynchronous with its own private `hbk::sys::EventLoop` in its own thread.
It offers the same methods as the asynchronous but also synchronous calls.
Synchronous calls dont have a result callback funtion. Instead the methods block until the response arrives and provide it in the return value.

It is easier to use the synchornous variants. But there are drawbacks:

- You have to take care that your code is thread safe!
- Only one request is in flight


