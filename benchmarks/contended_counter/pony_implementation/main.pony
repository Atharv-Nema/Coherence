use "collections"

actor Counter
  var _value: I64 = 0

  be increment() =>
    _value = _value + 1

actor Worker
  new create() => None

  be work(counter: Counter, num_incr: USize) =>
    var i: USize = 0
    while i < num_incr do
      counter.increment()
      i = i + 1
    end

actor Main
  new create(env: Env) =>
    let num_workers: USize = 10
    let num_incr: USize = 100000

    let counter = Counter

    let workers = Array[Worker](num_workers)

    var ind: USize = 0
    while ind < num_workers do
      workers.push(Worker)
      ind = ind + 1
    end

    ind = 0
    while ind < num_workers do
      try
        workers(ind)?.work(counter, num_incr)
      end
      ind = ind + 1
    end