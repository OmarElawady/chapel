// TODO: This would need to be moved somewhere more standard
enum IteratorType { solo, leader, follower };

config param compilerRewrite = true;

var A: [0..8] real;

def myIter(param flavor: IteratorType) var where flavor == IteratorType.solo {
  for i in 0..8 {
    yield A(i);
  }
}

def myIter(param flavor: IteratorType) where flavor == IteratorType.leader {
  yield 0..3;
  yield 4..5;
  yield 6..8;
}

def myIter(param flavor: IteratorType, followThis) var
  where flavor == IteratorType.follower {
  for i in followThis {
    yield A(i);
  }
}

if compilerRewrite {

  for a in myIter() do
    a = 1.1;

} else {

  for a in myIter(IteratorType.solo) do
    a = 1.1;

}
writeln("A is: ", A);


if compilerRewrite {

  forall a in myIter() {
    a = 1.2;
  }

} else {

  // TODO: Next step would be to move coforall into leader itself
  coforall blk in myIter(IteratorType.leader) {
    for a in myIter(IteratorType.follower, blk) {
      a = 1.2;
    }
  }

}

writeln("A is: ", A);
