LPMud Driver Internals
======================

> :pushpin: The contents of this article only applies to Neolith, which is different from original LPMud and MudOS.

# Starting Up

When the LPMud Driver process is started, it goes through the follow steps in sequence before begining accept connections:

1. Process command line arguments
2. Process configuration file
3. Initialize LPC virtual machine
4. Initialize LPC compiler
5. Load simul efun object (optional)
6. Load master object
7. Do epilogue


