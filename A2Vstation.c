/**
 *  Novation A-Station to V-Station patch converter
 * 
 *    Input: A-Station dump file (system exclusive message stored in *.syx)
 *    Output: V-Station readable dump file
 * 
 *  The K-Station can read A-Station's dump. The V-Station can read K-Station's dump.
 *  The V-Station is almost plug-in software version of K-Station, but it cannot read A-Station's dump.
 * 
 *  V/K Station has a data byte 126 in PROGRAM DATA BLOCK while A-Station is not used the byte(filled 0x00).
 *  This byte stores EFFECTS SELECT/KEYBOARD OCTAVE data.
 *   bit 0-2     0=Delay 1=reverb 2=chorus 3=distortion 4=EQ 5=panning 6=vocoder
 *   bit 3,4,5,6 Signed value 0 = Nominal Octave where middle C is 261Hz, -4 = Lowest Octave +5 = Highest octave
 *  For V-Station (which has no keyboard), only the bit 0-2 is effective. And 0x00 means EFFECTS=Delay.
 *  The EFFECTS in A/V/K Station is a kind of multi-effect, these effects can use simultaneously.
 *  So, the data byte 126 means a which effect is selected on panel (and ready to edit its parameter immediately).
 *  This doesn't mean which effect is used. The value difference makes no sound difference.
 * 
 *  So, this tool manipulates A-Station's dump ID to make it readable by V-Station.
 * 
 *  Compile example:
 *    [Linux]   gcc A2Vstation.c -Wall -O2 -o A2Vstation
 *    [Windows] cl /W3 /O2 A2Vstation.c
 * 
 *  Execution example:
 *    ./A2Vstation INPUT.syx OUTPUT.syx
 * 
**/
#define _CRT_SECURE_NO_WARNINGS	// Supress warnings for Visual studio

#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<inttypes.h>

#define	SOX	0xF0	// Start of exclusive
#define	NOVID1	0x00	// Novation ID 1
#define	NOVID2	0x20	// Novation ID 2
#define	NOVID3	0x29	// Novation ID 3
#define	DEVTYP	0x01	// Device Type
#define	ASTNID	0x40	// A-Station ID
#define	KSTNID	0x41	// K-Station and V-Station ID
#define	MSGTYP_CUR	0x00	// Current sound dump (sent from edit buffer)
#define	MSGTYP_PRG	0x01	// Program dump
#define	MSGTYP_PAI	0x02	// Program pair dump
#define	EOX 0xF7	// End of exclusive
#define	SIZE_PROGRAM_DATA	128	// Size of A/K/V station's single program(patch)
#define	OFFSET_PROGRAM	13	// Offset of actual program block start in system exclusive message
#define	SIZE_SYSEX_BUF	(SIZE_PROGRAM_DATA*2+OFFSET_PROGRAM+1)	// System exclusive message buffer, max size will be in the case of PROGRAM PAIR DUMP

/** Enumerations **/
enum ARGS{
	ARG_POS_EXECUTABLE,
	ARG_POS_INPUTFILE,
	ARG_POS_OUTPUTFILE
};

/** Prototypes **/
int main(int argc, char **argv);

/** Functions **/
int main(int argc, char **argv) {
	int ret_code = EXIT_SUCCESS;
	char infilename[1024], outfilename[1024];
	FILE *ifp, *ofp;
	uint8_t *sysExclMsg;
	uint8_t *bufAddr;
	
	// Check arguments
	if (argc <= ARG_POS_OUTPUTFILE) {
		fprintf(stderr, "Usage: %s INPUTFILE(.syx) OUTPUTFILE(.syx)\n", argv[ARG_POS_EXECUTABLE]);
		fprintf(stderr, "[Note] Supported only *.syx files, SMF(*.mid) is not supported.\n");
		ret_code = EXIT_FAILURE;
		return ret_code;
	}
	
	// Get filename
	strncpy(infilename, argv[ARG_POS_INPUTFILE], sizeof(infilename));
	strncpy(outfilename, argv[ARG_POS_OUTPUTFILE], sizeof(outfilename));
	
	// Open input file in binary read mode
	ifp = fopen(infilename, "rb");
	if(ifp == NULL){
		fprintf(stderr, "[ERR] File open error (%s)\n", infilename);
		ret_code = EXIT_FAILURE;
		return ret_code;
	}
	
	// Open output file in binary write mode
	ofp = fopen(outfilename, "wb");
	if(ofp == NULL){
		fprintf(stderr, "[ERR] File open error (%s)\n", outfilename);
		ret_code = EXIT_FAILURE;
		return ret_code;
	}
	
	// Allocate memory for System Exclusive message buffer
	sysExclMsg = (uint8_t *) malloc(sizeof(uint8_t) * SIZE_SYSEX_BUF);
	bufAddr = sysExclMsg;
	
	// Read file
	int counter = -1;
	while(fread(bufAddr, sizeof(uint8_t), 1, ifp) == 1){
		// Check the first byte to detect syx file or not
		if( counter == -1 ){
			if( *bufAddr != SOX ){
				fprintf(stderr, "[ERR] %s is not *.syx file\n", infilename);
				ret_code = EXIT_FAILURE;
				return ret_code;
			}
		}
		
		// Counter management
		if( *bufAddr == SOX ){
			counter = 0;
		}
		else{
			counter++;
		}
		
		// Process at EOX
		if( *bufAddr == EOX ){
			// Verify Novation ID
			if(*(sysExclMsg+1) != NOVID1){
				fprintf(stderr, "[ERR] Unknown data (%02x), it should be %02x\n", *(sysExclMsg+1), NOVID1);
				ret_code = EXIT_FAILURE;
				return ret_code;
			}
			if(*(sysExclMsg+2) != NOVID2){
				fprintf(stderr, "[ERR] Unknown data (%02x), it should be %02x\n", *(sysExclMsg+2), NOVID2);
				ret_code = EXIT_FAILURE;
				return ret_code;
			}
			if(*(sysExclMsg+3) != NOVID3){
				fprintf(stderr, "[ERR] Unknown data (%02x), it should be %02x\n", *(sysExclMsg+3), NOVID3);
				ret_code = EXIT_FAILURE;
				return ret_code;
			}
			
			// Verify Device Type
			if(*(sysExclMsg+4) != DEVTYP){
				fprintf(stderr, "[ERR] Unknown data (%02x), it should be %02x\n", *(sysExclMsg+4), DEVTYP);
				ret_code = EXIT_FAILURE;
				return ret_code;
			}
			
			// Verify A-Station ID
			if(*(sysExclMsg+5) == KSTNID){
				fprintf(stderr, "[ERR] The input data is V-Station/K-Station dump. No conversion required.\n");
				ret_code = EXIT_FAILURE;
				return ret_code;
			}
			else if(*(sysExclMsg+5) != ASTNID){
				fprintf(stderr, "[ERR] Unknown data (%02x), it should be %02x\n", *(sysExclMsg+5), ASTNID);
				ret_code = EXIT_FAILURE;
				return ret_code;
			}
			else{
				// Repalce A-Station ID with V-Station ID
				*(sysExclMsg+5) = KSTNID;
			}
			
			// Verify Message Type
			switch(*(sysExclMsg+7)){
				case MSGTYP_CUR:
					fprintf(stdout, "[INFO] Current sound (edit buffer) dump\n");
					break;
				case MSGTYP_PRG:
					if(*(sysExclMsg+8) == 0){
						fprintf(stdout, "[INFO] Current selected bank, PROGRAM NUMBER=%d\n", *(sysExclMsg+12));
					}
					else if(*(sysExclMsg+8) == 1){
						fprintf(stdout, "[INFO] PROGRAM BANK=%d, PROGRAM NUMBER=%d\n", *(sysExclMsg+11), *(sysExclMsg+12));
					}
					break;
				case MSGTYP_PAI:
					if(*(sysExclMsg+8) == 0){
						// In A-Station, destination is "Current selected bank". But V-Station seems ignore C byte(destination bank control).
						fprintf(stdout, "[INFO] PROGRAM BANK=%d, PROGRAM NUMBER=%d and %d\n", *(sysExclMsg+11), *(sysExclMsg+12), *(sysExclMsg+12)+1);
					}
					else if(*(sysExclMsg+8) == 1){
						fprintf(stdout, "[INFO] PROGRAM BANK=%d, PROGRAM NUMBER=%d and %d\n", *(sysExclMsg+11), *(sysExclMsg+12), *(sysExclMsg+12)+1);
					}
					break;
				default:	// If input file is any kind of A-Station's PROGRAM (PATCH) dump, do not fall into here.
					break;
			}
			
			// Write to file
			for(int i=0; i<=counter; i++){
				if(fwrite(sysExclMsg+i, sizeof(uint8_t), 1, ofp) != 1){
					fprintf(stderr, "[ERR] File write error\n");
					ret_code = EXIT_FAILURE;
					return ret_code;
				};
			}
			// Initialize buffer address
			bufAddr = sysExclMsg;
		}
		else{
			bufAddr++;
		}
	}
	
	// Close file
	fclose(ofp);
	fclose(ifp);
	
	free(sysExclMsg);
	
	return ret_code;
}