// This Pulsar Source file will generate
//  the smallest Neutron file that produces a value.
// Compile & Run with:
// $ pulsar-tools compile --no-debug ./00-smallest-neutron.pls
// $ pulsar-tools run ./00-smallest-neutron.ntx
*(main args) -> 1: 0 .

// NOTE: The Pulsar parser puts an explicit
//       return instruction at the end of each function,
//       which is not necessary for the correct execution
//       of the program. So the file could be 2 bytes smaller.
