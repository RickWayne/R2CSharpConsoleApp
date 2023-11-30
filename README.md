# R2CSharpConsoleApp
Visual Studio .NET 8 console application that instantiates a Rusle2 model from the DLL and runs it.

You will need to supply your own GDB. I think the solution still sets command-line arguments but those are unused. The way we do the R2 instance life-cycle is needlessly abstruse and an artifact of some poor thinking I did about three years ago -- sorry!

Rusle2.cs has the link to the unmanaged-code-invocation documentation.

Hope this helps!
