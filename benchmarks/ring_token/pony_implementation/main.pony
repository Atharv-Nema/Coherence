use "collections"

// The "Hot Potato" - a simple object with exclusive access (iso)
class Potato
  var counter: U64
  new create(c: U64) =>
    counter = c

actor Worker
  let _id: U64
  var _next: (Worker | None) = None
  
  new create(id: U64) =>
    _id = id

  be set_next(neighbor: Worker) =>
    _next = neighbor

  be pass_potato(p: Potato iso) =>
    if p.counter > 0 then
      p.counter = p.counter - 1
      match _next
      | let n: Worker =>
        n.pass_potato(consume p)
      end
    end

actor Main
  new create(env: Env) =>
    // Change n_actors to USize for Array and Range compatibility
    let n_actors: USize = 100
    let m_passes: U64 = 1_000_000
    
    let workers = Array[Worker](n_actors)
    
    // 1. Create the actors
    // Use Range[USize] explicitly to ensure 'i' is the right type
    for i in Range[USize](0, n_actors) do
      // Worker takes U64, so convert i (USize) to U64
      workers.push(Worker(i.u64()))
    end
    
    // 2. Link them in a circle
    try
      for i in Range[USize](0, n_actors) do
        let current = workers(i)?
        let next_actor = workers((i + 1) % n_actors)?
        current.set_next(next_actor)
      end
      
      // 3. Start the clock and toss the potato
      workers(0)?.pass_potato(recover Potato(m_passes) end)
    end