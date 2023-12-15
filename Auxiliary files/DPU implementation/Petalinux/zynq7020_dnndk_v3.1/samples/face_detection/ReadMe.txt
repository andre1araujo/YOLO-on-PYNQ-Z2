<<<<<<< HEAD
version https://git-lfs.github.com/spec/v1
oid sha256:928414ee4dd7af13dec8b4660e196f58a4900b1cdf607a4b91026c9d00ebd3f5
size 438
=======
1. Copy xilinx_dnndk_v3.1/host_x86/eth-show folder to one Windows host computer
2. Copy the sample xilinx_dnndk_v3.1/ZedBoard/samples/face_detection to ZedBoard
3. Make sure ZedBoard and the host computer are on the same netword segment
4. Modity the value of eth_show_ip in src/main.cc of ZedBoard to the ip address of host computer, and run make
5. Run eth-show/eth_show.exe on the host computer
6. Run ./face_detection on the ZedBoard
>>>>>>> origin/main
