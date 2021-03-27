.PHONY: all compile io1 io2 io3 io4 io5 io6 io7 io8

all: compile

compile:
	@g++ main.cpp -o main.o -lpthread
io1:
	@./main.o ./inputs/configuration_file_1.txt ./output1.txt
io2:
	@./main.o ./inputs/configuration_file_2.txt ./output2.txt
io3:
	@./main.o ./inputs/configuration_file_3.txt ./output3.txt
io4:
	@./main.o ./inputs/configuration_file_4.txt ./output4.txt
io5:
	@./main.o ./inputs/configuration_file_5.txt ./output5.txt
io6:
	@./main.o ./inputs/configuration_file_6.txt ./output6.txt
io7:
	@./main.o ./inputs/configuration_file_7.txt ./output7.txt
io8:
	@./main.o ./inputs/configuration_file_8.txt ./output8.txt
