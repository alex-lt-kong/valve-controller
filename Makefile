SRC_DIR = ./src

main: $(SRC_DIR)/vc.c $(SRC_DIR)/utils.o cam.so
	gcc $(SRC_DIR)/vc.c $(SRC_DIR)/utils.o -O3 -o ./vc.out -lonion -lpthread -ljson-c -lpigpio -Wall -l:cam.so -lsqlite3

cam.so: $(SRC_DIR)/cam.cpp $(SRC_DIR)/cam.h utils.o
	g++ -shared -o /usr/local/lib/cam.so $(SRC_DIR)/cam.cpp $(SRC_DIR)/utils.o -I/usr/local/include/opencv4 -lopencv_videoio -lopencv_imgproc -lopencv_core -Wall -O3

utils.o: $(SRC_DIR)/utils.c $(SRC_DIR)/utils.h
	gcc $(SRC_DIR)/utils.c -c -O3 -Wall


clean:
	rm *.out $(SRC_DIR)/*.o