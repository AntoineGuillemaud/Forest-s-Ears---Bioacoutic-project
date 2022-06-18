
#include <iostream>
#include <fstream>

using namespace std;

long counter=52;

int temp = 0;
int offset = 0;

char filename1[50];
char filename2[50];

int main () {

	for (int month = 0; month <= 12; month++) {

		for (int day = 0; day <= 31; day++) {

			for (int hour = 0; hour <= 23; hour++) {

				for (int minute = 0; minute <= 59; minute++) {
					snprintf(filename1, 50, "audio-m%02d-j%02d_%02dh%02d.bin", month, day, hour, minute);
					ifstream myfile1(filename1, ios::in | ios::ate | ios::binary);
					if (myfile1.is_open()) {

						
						snprintf(filename2, 50, "audio-j%02d-m%02d_%02dh%02d.wav", day, month, hour, minute);
						ofstream myfile2;
						myfile2.open(filename2, ios::out | ios::trunc | ios::binary);


						streampos size;
						char* memblock;

						if (myfile1.is_open())
						{
							size = myfile1.tellg();
							memblock = new char[size];
							myfile1.seekg(0, ios::beg);
							myfile1.read(memblock, size);
							myfile1.close();

							while (counter < size) {
								temp = (memblock[counter] + memblock[counter + 1] * 256) + 16384; 
								memblock[counter] = (temp & 0x00ff);
								memblock[counter + 1] = (temp & 0xff00) >> 8;
								counter+=2;
							}
							counter = 52;

							myfile2.write(memblock, size);

							delete[] memblock;
							myfile2.close();
						}
						else cout << "Unable to open file";

						cout << filename1 << "\n";




					}
				}
			}
		}
	}


  return 0;
}
