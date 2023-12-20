1. Copy xilinx_dnndk_v3.1/host_x86/eth-show folder to one Windows host computer
2. Copy the sample xilinx_dnndk_v3.1/ZedBoard/samples/face_detection to ZedBoard
3. Make sure ZedBoard and the host computer are on the same netword segment
4. Modity the value of eth_show_ip in src/main.cc of ZedBoard to the ip address of host computer, and run make
5. Run eth-show/eth_show.exe on the host computer
6. Run ./face_detection on the ZedBoard
