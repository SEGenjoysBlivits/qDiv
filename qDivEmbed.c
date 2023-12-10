/*
qDivEmbed - A compiling utility for embedding game assets
Copyright (C) 2023  Gabriel F. Hodges

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/
#include<stdio.h>
#include<stdlib.h>
#include<string.h>

size_t currentPointer = 0;
char elementHeader[10000000];

void addFile(char* fileName) {
	char filePath[1024];
	sprintf(filePath, "elements/%s", fileName);
	FILE* fileIQ = fopen(filePath, "r");
	if(fileIQ != NULL) {
		static char bufferTMP[1024];
		fseek(fileIQ, 0L, SEEK_END);
		int fileSZ = ftell(fileIQ);
		fseek(fileIQ, 0L, SEEK_SET);
		unsigned char* fileData = (unsigned char*)malloc(fileSZ);
		fread(fileData, 1, fileSZ, fileIQ);
		sprintf(bufferTMP, "\nconst size_t %sSZ = %d;\nconst unsigned char %s[] = {", fileName, fileSZ, fileName);
		size_t byteSL = 0;
		while(byteSL < 1024) {
			if(bufferTMP[byteSL] == '.' || bufferTMP[byteSL] == '/') bufferTMP[byteSL] = '_';
			byteSL++;
		}
		strcpy(elementHeader + currentPointer, bufferTMP);
		currentPointer += strlen(bufferTMP);
		byteSL = 0;
		int columnSL = 0;
		while(byteSL < fileSZ - 1) {
			if(fileData[byteSL] >= 0x10) {
				sprintf(elementHeader + currentPointer, "0x%x,", fileData[byteSL]);
			}else{
				sprintf(elementHeader + currentPointer, "0x0%x,", fileData[byteSL]);
			}
			currentPointer += 5;
			byteSL++;
			columnSL++;
			if(columnSL == 32) {
				columnSL = 0;
				elementHeader[currentPointer] = '\n';
				currentPointer += 1;
			}
		}
		if(fileData[byteSL] >= 0x10) {
			sprintf(elementHeader + currentPointer, "0x%x};\n", fileData[byteSL]);
		}else{
			sprintf(elementHeader + currentPointer, "0x0%x};\n", fileData[byteSL]);
		}
		currentPointer += 7;
		fclose(fileIQ);
	}
}

int main() {
	addFile("shaders/vertexshader.glsl");
	addFile("shaders/fragmentshader.glsl");
	addFile("logo.png");
	addFile("text.png");
	addFile("interface.png");
	addFile("selection.png");
	addFile("floor0.png");
	addFile("floor1.png");
	addFile("floor2.png");
	addFile("floor3.png");
	addFile("wall.png");
	addFile("foundation.png");
	addFile("dummy.png");
	addFile("blank.png");
	addFile("artifacts/old_slicer.png");
	addFile("artifacts/old_swinger.png");
	addFile("artifacts/block.png");
	addFile("artifacts/plastic.png");
	addFile("artifacts/lamp.png");
	FILE* fileIQ = fopen("elements.h", "w");
	fwrite(elementHeader, 1, currentPointer, fileIQ);
	fclose(fileIQ);
}
