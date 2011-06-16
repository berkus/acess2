/*
 * Acess2 Library Suite
 * - Readline
 * 
 * Text mode entry with history
 */
#include <readline.h>
#include <acess/sys.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define STDIN_FD	0
#define STDOUT_FD	1

// Size of the read() buffer
// - 32 should be pleanty
#define READ_BUFFER_SIZE	32

// === STRUCTURES ===
struct sReadline
{
	 int	UseHistory;	// Boolean
	
	// TODO: Command Completion
	
	// History
	 int	NumHistory;
	char	**History;
	
	// Internal Flags
	char	*OutputValue;	//!< Pointer (owned by history) to output value
	
	// Command Buffer
	 int	BufferSize;	// Allocated size of the buffer
	 int	BufferUsed; 	// Offset of first free byte in the buffer
	 int	BufferWritePos;	// Cursor location
	char	*CurBuffer;	// Current translated command (pointer to a entry in history)
	
	// 
	 int	HistoryPos;
	
	// Read Buffer
	char	ReadBuffer[READ_BUFFER_SIZE];	//!< Buffer for read()
	 int	ReadBufferLen;
};

// === PROTOTYPES ===
 int	SoMain();
tReadline	*Readline_Init(int bUseHistory);
 int	Readline_int_ParseCharacter(tReadline *Info, char *Input);
char	*Readline_NonBlock(tReadline *Info);
char	*Readline(tReadline *Info);

// === GLOBALS ===

// === CODE ===
int SoMain()
{
	return 0;
}

tReadline *Readline_Init(int bUseHistory)
{
	tReadline	*ret = calloc( 1, sizeof(tReadline) );
	ret->UseHistory = bUseHistory;
	ret->BufferSize = 0;
	
	ret->History = malloc( 1 * sizeof(*ret->History) );
	ret->History[0] = NULL;
	ret->NumHistory = 1;
	
	return ret;
}

/**
 */
char *Readline_NonBlock(tReadline *Info)
{
	 int	len, i;
	
	// Read as much as possible (appending to remaining data)
	len = read(STDIN_FD, READ_BUFFER_SIZE - 1 - Info->ReadBufferLen, Info->ReadBuffer);
	Info->ReadBuffer[Info->ReadBufferLen + len] = '\0';
	
	// Parse the data we have
	for( i = 0; i < len; )
	{
		int size = Readline_int_ParseCharacter(Info, Info->ReadBuffer+i);
		if( size <= 0 )	break;	// Error, skip the rest?
		i += size;
	}
	
	// Move the unused data to the start of the buffer
	memcpy(Info->ReadBuffer, &Info->ReadBuffer[Info->ReadBufferLen + i], len - i);
	Info->ReadBufferLen = len - i;
	
	// Is the command finished?
	if( Info->OutputValue ) {
		char	*ret = Info->OutputValue;
		Info->OutputValue = NULL;	// Mark as no command pending
		return ret;	// Return the string (now the caller's responsibility)
	}
	
	// Return NULL when command is still being edited
	return NULL;
}

char *Readline(tReadline *Info)
{
	char	*ret;
	
	while( NULL == (ret = Readline_NonBlock(Info)) );
	
	return ret;
}

int Readline_int_AddToHistory(tReadline *Info, const char *String)
{
	void	*tmp;
	
	// History[#-1] = CurBuffer (always)
	if( !Info->History ) {
		// Realy shouldn't happen, but just in case
		Info->History = malloc( sizeof(char*) );
		Info->NumHistory = 1;
	}
	
	// Don't add duplicates
	if( Info->NumHistory >= 2 && strcmp( Info->History[ Info->NumHistory-2 ], String ) == 0 )
	{
		return 0;
	}
	
	// Duplicate over the current
	Info->History[ Info->NumHistory-1 ] = strdup(String);
	
	Info->NumHistory ++;
	
	tmp = realloc( Info->History, Info->NumHistory * sizeof(char*) );
	if( tmp == NULL )	return -1;
	Info->History = tmp;
				
	// Zero the new current
	Info->History[ Info->NumHistory-1 ] = NULL;
	
	return 0;
}

int Readline_int_ParseCharacter(tReadline *Info, char *Input)
{
	 int	ofs = 0;
	char	ch;
	
	if( Input[ofs] == 0 )	return 0;
	
	// Read In Command Line
	ch = Input[ofs++];
	
	if(ch == '\n')
	{
//		printf("\n");
		if(Info->CurBuffer)
		{	
			// Cap String
			Info->CurBuffer[Info->BufferUsed] = '\0';
			
			if( Info->UseHistory )
				Readline_int_AddToHistory(Info, Info->CurBuffer);
			Info->OutputValue = strdup(Info->CurBuffer);
		}
		else
			Info->OutputValue = strdup("");
		
		// Save and reset
		Info->BufferSize = 0;
		Info->BufferUsed = 0;
		Info->BufferWritePos = 0;
		Info->CurBuffer = 0;
		Info->HistoryPos = Info->NumHistory - 1;
		
		return 1;
	}
	
	switch(ch)
	{
	// Control characters
	case '\x1B':
		ch = Input[ofs++];	// Read control character
		switch(ch)
		{
		//case 'D':	if(pos)	pos--;	break;
		//case 'C':	if(pos<len)	pos++;	break;
		case '[':
			ch = Input[ofs++];	// Read control character
			switch(ch)
			{
			case 'A':	// Up
				{
					 int	oldLen = Info->BufferUsed;
					 int	pos;
					if( Info->HistoryPos <= 0 )	break;
					
					// Move to the beginning of the line
					pos = oldLen;
					while(pos--)	write(STDOUT_FD, 3, "\x1B[D");
					
					// Update state
					Info->CurBuffer = Info->History[--Info->HistoryPos];
					Info->BufferSize = Info->BufferUsed = strlen(Info->CurBuffer);
					
					write(STDOUT_FD, Info->BufferUsed, Info->CurBuffer);
					Info->BufferWritePos = Info->BufferUsed;
					
					// Clear old characters (if needed)
					if( oldLen > Info->BufferWritePos ) {
						pos = oldLen - Info->BufferWritePos;
						while(pos--)	write(STDOUT_FD, 1, " ");
						pos = oldLen - Info->BufferWritePos;
						while(pos--)	write(STDOUT_FD, 3, "\x1B[D");
					}
				}
				break;
			case 'B':	// Down
				{
					 int	oldLen = Info->BufferUsed;
					 int	pos;
					if( Info->HistoryPos >= Info->NumHistory - 1 )	break;
					
					// Move to the beginning of the line
					pos = oldLen;
					while(pos--)	write(STDOUT_FD, 3, "\x1B[D");
					
					// Update state
					Info->CurBuffer = Info->History[Info->HistoryPos++];
					Info->BufferSize = Info->BufferUsed = strlen(Info->CurBuffer);
					
					// Write new line
					write(STDOUT_FD, Info->BufferUsed, Info->CurBuffer);
					Info->BufferWritePos = Info->BufferUsed;
					
					// Clear old characters (if needed)
					if( oldLen > Info->BufferWritePos ) {
						pos = oldLen - Info->BufferWritePos;
						while(pos--)	write(STDOUT_FD, 1, " ");
						pos = oldLen - Info->BufferWritePos;
						while(pos--)	write(STDOUT_FD, 3, "\x1B[D");
					}
				}
				break;
			case 'D':	// Left
				if(Info->BufferWritePos == 0)	break;
				Info->BufferWritePos --;
				write(STDOUT_FD, 3, "\x1B[D");
				break;
			case 'C':	// Right
				if(Info->BufferWritePos == Info->BufferUsed)	break;
				Info->BufferWritePos ++;
				write(STDOUT_FD, 3, "\x1B[C");
				break;
			}
		}
		break;
	
	// Backspace
	case '\b':
		if(Info->BufferWritePos <= 0)	break;	// Protect against underflows
		// Write the backsapce
		write(STDOUT_FD, 1, &ch);
		if(Info->BufferWritePos == Info->BufferUsed)	// Simple case: End of string
		{
			Info->BufferUsed --;
			Info->BufferWritePos --;
		}
		else
		{
			// Have to delete the character, and reposition the text
			char	buf[7] = "\x1B[000D";
			 int	delta = Info->BufferUsed - Info->BufferWritePos + 1;
			buf[2] += (delta/100) % 10;
			buf[3] += (delta/10) % 10;
			buf[4] += (delta) % 10;
			// Write everything save for the deleted character
			write(STDOUT_FD,
				Info->BufferUsed - Info->BufferWritePos,
				&Info->CurBuffer[Info->BufferWritePos]
				);
			ch = ' ';	write(STDOUT_FD, 1, &ch);	ch = '\b';	// Clear old last character
			write(STDOUT_FD, 7, buf);	// Update Cursor
			// Alter Buffer
			memmove(&Info->CurBuffer[Info->BufferWritePos-1],
				&Info->CurBuffer[Info->BufferWritePos],
				Info->BufferUsed-Info->BufferWritePos
				);
			Info->BufferWritePos --;
			Info->BufferUsed --;
		}
		break;
	
	// Tab
	case '\t':
		//TODO: Implement Tab-Completion
		//Currently just ignore tabs
		break;
	
	default:		
		// Expand Buffer
		if(Info->BufferUsed + 1 > Info->BufferSize)
		{
			Info->BufferSize += 256;
			Info->CurBuffer = Info->History[Info->HistoryPos]
				= realloc(Info->CurBuffer, Info->BufferSize);
			if(!Info->CurBuffer)	return 0;
		}
		
		// Editing inside the buffer
		if(Info->BufferWritePos < Info->BufferUsed) {
			char	buf[7] = "\x1B[000D";
			 int	delta = Info->BufferUsed - Info->BufferWritePos;
			buf[2] += (delta/100) % 10;
			buf[3] += (delta/10) % 10;
			buf[4] += (delta) % 10;
			write(STDOUT_FD, 1, &ch);	// Print new character
			write(STDOUT_FD,
				Info->BufferUsed - Info->BufferWritePos,
				&Info->CurBuffer[Info->BufferWritePos]
				);
			write(STDOUT_FD, 7, buf);	// Update Cursor
			// Move buffer right
			memmove(
				&Info->CurBuffer[Info->BufferWritePos+1],
				&Info->CurBuffer[Info->BufferWritePos],
				Info->BufferUsed - Info->BufferWritePos
				);
		}
		// Simple append
		else {
			write(STDOUT_FD, 1, &ch);
		}
		Info->CurBuffer[ Info->BufferWritePos ++ ] = ch;
		Info->BufferUsed ++;
		break;
	}
	
	return ofs;
}
