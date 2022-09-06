Contains a few C code snippets which you can use to run on the FPGA -> Should be able to compile on its own via just normal compile tools

HLS tools use SDSoc, but all of these have been disabled in the examples:
To use the HLS tools - which are more useful for the parralel codes you may need to comment out some of the code:
Or head to the examples:
https://github.com/Xilinx/SDSoC_Examples

Uses memory buffers as seen in:
https://github.com/ikwzm/udmabuf
(Remember to load the pump as in the examples and have 8 pumps setup: respectively "udmabuf0, udmabuf1, ... "

