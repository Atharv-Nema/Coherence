use "collections"

actor Peer
  let _id: I32
  var _peers: Array[Peer] val
  let _num_peers: I32
  var _pings_remaining: I32

  new create(id_arg: I32, n: I32, m: I32) =>
    _id = id_arg
    _peers = recover val Array[Peer] end
    _num_peers = n
    _pings_remaining = m

  be set_peer_list(peers_arg: Array[Peer] val) =>
    _peers = peers_arg

  // Pseudorandom function
  fun random_gen(): USize =>
    let mixed: I32 = ((_id + 1) * 1000003) + (_pings_remaining * 500009)
    (mixed % _num_peers).usize()

  fun ref _send_next_ping() =>
    if _pings_remaining > 0 then
      let target_idx = random_gen()
      try
        _peers(target_idx)?.receive_ping(this)
        _pings_remaining = _pings_remaining - 1
      end
    end

  be start() =>
    _send_next_ping()

  be receive_ping(from: Peer) =>
    from.receive_pong()

  be receive_pong() =>
    _send_next_ping()

actor Main
  new create(env: Env) =>
    var n: I32 = 1000
    var m: I32 = 100

    let peer_list_iso = recover iso Array[Peer](n.usize()) end
    var i: I32 = 0
    while i < n do
      peer_list_iso.push(Peer(i, n, m))
      i = i + 1
    end
    let peer_list: Array[Peer] val = consume peer_list_iso
    i = 0
    while i < n do
      try
        peer_list(i.usize())?.set_peer_list(peer_list)
      end
      i = i + 1
    end
    i = 0
    while i < n do
      try
        peer_list(i.usize())?.start()
      end
      i = i + 1
    end
