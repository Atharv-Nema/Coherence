class Point
  var x: I32 = 1
  var y: I32 = 2
  var z: I32 = 3

actor Main
  new create(env: Env) =>
    let p = Point
    var num_ops: I32 = 100000000
    var i: I32 = 0
    while i < num_ops do
      p.x = p.x + 1
      p.y = (p.x + p.y) % 10
      p.z = p.z + p.y
      i = i + 1
    end
    env.out.print(p.z.string())