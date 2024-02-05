#include<stdio.h>
#include<stdint.h>
#include<stddef.h>
#include<string.h>
#include<stdlib.h>
#include<stdbool.h>
#include<math.h>
#define QSM_FAILED -1
#define QSM_NOVALUE 0
#define QSM_IMPLICIT_STR 1
#define QSM_EXPLICIT_STR 2
#define QSM_SIGNED_INT 3
#define QSM_UNSIGNED_INT 4

size_t QSMread(int8_t* filePath, int8_t** buffer) {
	FILE* fileIQ = fopen(filePath, "r");
	if(fileIQ == NULL) {
		*buffer = NULL;
		return 0;
	}
	fseek(fileIQ, 0L, SEEK_END);
	size_t bufferSZ = ftell(fileIQ);
	fseek(fileIQ, 0L, SEEK_SET);
	*buffer = malloc(bufferSZ);
	fread(*buffer, 1, bufferSZ, fileIQ);
	fclose(fileIQ);
	return bufferSZ;
}

size_t QSMencode(int32_t* result, void* value, int8_t* buffer, size_t bufferSZ, size_t* entryPT, int8_t* tagIQ, size_t tagSZ) {
	if(buffer == NULL || value == NULL || tagIQ == NULL) {
		value = NULL;
		*result = QSM_FAILED;
		return 0;
	}
	size_t byteIQ = entryPT == NULL ? 0 : *entryPT;
	size_t byteSL = byteIQ;
	do{
		if(buffer[byteSL] == '<') {
			if(byteSL + tagSZ + 1 >= bufferSZ) {
				value = NULL;
				*result = QSM_FAILED;
				return 0;
			}
			byteSL++;
			if(buffer[byteSL + tagSZ] == '>' && !memcmp(buffer + byteSL, tagIQ, tagSZ)) {
				byteSL += tagSZ + 1;
				int8_t* valueIQ = NULL;
				size_t lengthIQ = 0;
				int32_t factor = 1;
				*result = QSM_NOVALUE;
				do{
					if(buffer[byteSL] == '"') {
						if(*result == QSM_NOVALUE) {
							*result = QSM_EXPLICIT_STR;
						}else if(*result == QSM_EXPLICIT_STR){
							break;
						}else{
							lengthIQ++;
							valueIQ = realloc(valueIQ, lengthIQ);
							valueIQ[lengthIQ - 1] = buffer[byteSL];
						}
					}else if(*result == QSM_EXPLICIT_STR || (buffer[byteSL] >= 'A' && buffer[byteSL] <= 'Z') ||
					(buffer[byteSL] >= 'a' && buffer[byteSL] <= 'z')) {
						lengthIQ++;
						valueIQ = realloc(valueIQ, lengthIQ);
						valueIQ[lengthIQ - 1] = buffer[byteSL];
						if(*result != QSM_EXPLICIT_STR) *result = QSM_IMPLICIT_STR;
					}else if(buffer[byteSL] >= '0' && buffer[byteSL] <= '9') {
						lengthIQ++;
						valueIQ = realloc(valueIQ, lengthIQ);
						valueIQ[lengthIQ - 1] = buffer[byteSL];
						if(*result == QSM_NOVALUE) *result = QSM_SIGNED_INT;
					}else if(buffer[byteSL] == '#') {
						if(*result == QSM_NOVALUE) {
							*result = QSM_UNSIGNED_INT;
						}else{
							*result = QSM_IMPLICIT_STR;
						}
					}else if(buffer[byteSL] == '-') {
						if(*result == QSM_NOVALUE) {
							*result = QSM_SIGNED_INT;
							factor = -1;
						}else{
							*result = QSM_IMPLICIT_STR;
						}
					}else if(buffer[byteSL] == '<' || *result != QSM_NOVALUE) {
						break;
					}
					byteSL++;
					if(byteSL == bufferSZ) byteSL = 0;
				}while(byteSL != byteIQ);
				size_t resultSZ = 0;
				int32_t digitSL;
				switch(*result) {
					case QSM_IMPLICIT_STR:
					case QSM_EXPLICIT_STR:
						lengthIQ++;
						valueIQ = realloc(valueIQ, lengthIQ);
						valueIQ[lengthIQ - 1] = '\0';
						memcpy(value, valueIQ, lengthIQ);
						resultSZ = lengthIQ;
						break;
					case QSM_SIGNED_INT:
						int32_t valueInt = 0;
						digitSL = (int32_t)lengthIQ - 1;
						while(digitSL > -1) {
							valueInt += (valueIQ[digitSL] - '0') * pow(10, (int32_t)lengthIQ - digitSL - 1);
							digitSL--;
						}
						*((int32_t*)value) = valueInt * factor;
						resultSZ = sizeof(int32_t);
						break;
					case QSM_UNSIGNED_INT:
						uint32_t valueUint = 0;
						digitSL = (int32_t)lengthIQ - 1;
						while(digitSL > -1) {
							valueUint += (valueIQ[digitSL] - '0') * pow(10, (int32_t)lengthIQ - digitSL - 1);
							digitSL--;
						}
						*((uint32_t*)value) = valueUint;
						resultSZ = sizeof(uint32_t);
						break;
				}
				free(valueIQ);
				return resultSZ;
			}
		}
		byteSL++;
		if(byteSL == bufferSZ) byteSL = 0;
	}while(byteSL != byteIQ);
}

size_t QSMdecode(int32_t option, void* value, size_t valueSZ, int8_t** buffer, size_t bufferSZ, int8_t* tagIQ, size_t tagSZ) {
	if(option == QSM_FAILED || tagIQ == NULL) return bufferSZ;
	size_t totalSZ = bufferSZ + valueSZ + tagSZ + 4 + (option != QSM_NOVALUE) + (option == QSM_UNSIGNED_INT);
	*buffer = realloc(*buffer, totalSZ);
	switch(option) {
		case QSM_NOVALUE:
			sprintf((*buffer) + bufferSZ, "<%s>\n", tagIQ);
			break;
		case QSM_IMPLICIT_STR:
			sprintf((*buffer) + bufferSZ, "<%s> %s\n", tagIQ, (int8_t*)value);
			break;
		case QSM_SIGNED_INT:
			sprintf((*buffer) + bufferSZ, "<%s> %d\n", tagIQ, *((int32_t*)value));
			break;
		case QSM_UNSIGNED_INT:
			sprintf((*buffer) + bufferSZ, "<%s> #%u\n", tagIQ, *((uint32_t*)value));
			break;
	}
	return totalSZ;
}

void QSMwrite(int8_t* filePath, int8_t** buffer, size_t bufferSZ) {
	FILE* fileIQ = fopen(filePath, "w");
	if(fileIQ == NULL) {
		free(*buffer);
		return;
	}
	fwrite(*buffer, 1, bufferSZ, fileIQ);
	fclose(fileIQ);
	free(*buffer);
}
