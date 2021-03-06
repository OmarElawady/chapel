use Random;

config const nBytes = 100000;
config const nIterations = 25;
config const useFactory = false;

var randomStream = createRandomStream(eltType=uint(8));
for i in 1..nIterations {
  // create bytes with random bytes
  var buf = c_malloc(uint(8), nBytes+1);
  for i in 0..#nBytes {
    buf[i] = randomStream.getNext();
  }
  buf[nBytes] = 0;

  const randomBytes = createBytesWithOwnedBuffer(buf, length=nBytes,
                                                      size=nBytes+1);

  if randomBytes.length != nBytes {
    halt("Error creating bytes object with correct length");
  }

  var randomEscapedString: string;
  try {
     randomEscapedString =
        if useFactory then
          createStringWithNewBuffer(buf, length=nBytes, size=nBytes+1,
                                    errors=decodePolicy.escape)
        else
          randomBytes.decode(errors=decodePolicy.escape);
  }
  catch e: DecodeError {
    halt("Unexpected decode error");
  }
  catch {
    halt("Unexpected error");
  }

  // unescaped string must be equal to the initial `bytes`
  if randomEscapedString.encode(errors=encodePolicy.unescape) != randomBytes {
    halt("Failed. Seed:", randomStream.seed, " Iteration:" , i);
  }
}
writeln("Success");

