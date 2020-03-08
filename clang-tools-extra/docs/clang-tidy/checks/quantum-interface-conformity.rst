.. title:: clang-tidy - quantum-interface-conformity

quantum-interface-conformity
============================

The pattern detect interfaces (class that start with an uppercase I and have an uppercase letter after) that don't follow the Quantum code guidelines:
	- No virtual destructor.
	- Non-pure methods (inherited or declared inside the class).
