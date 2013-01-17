#define _WIN32
#define CINTERFACE 1

#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "anim.h"

char AnimFile[200],OutputFile[200];
long CompressionType=1;
long FirstFrame=0;
long StartX=0,StartY=0;
long StartW=-1,StartH=-1;
long TransColor=-1;

char Line[300];
int ParseCommandLine(LPSTR  lpCmdLine)
{
	char *Token;

	memcpy(&Line[0],lpCmdLine,strlen(lpCmdLine)+1);

	Token=strtok(&Line[0]," \t\n\r,");
	if(Token == NULL)
	{
		printf("No Parameter file specified\n");
		return(0);
	}
	sprintf(AnimFile,"%s",Token);
	Token=strtok(NULL," \t\n\r,");
	if(Token == NULL)
	{
		printf("No output file specified\n");
		return(0);
	}
	sprintf(OutputFile,"%s",Token);
	Token=strtok(NULL," \t\n\r,");
	while(Token != NULL)
	{
		if(!stricmp(Token,"/x"))
		{
			Token=strtok(NULL," \t\n\r,");
			if(Token != NULL)
				StartX=atol(Token);
		}
		else if(!stricmp(Token,"/y"))
		{
			Token=strtok(NULL," \t\n\r,");
			if(Token != NULL)
				StartY=atol(Token);
		}
		else if(!stricmp(Token,"/w"))
		{
			Token=strtok(NULL," \t\n\r,");
			if(Token != NULL)
				StartW=atol(Token);
		}
		else if(!stricmp(Token,"/h"))
		{
			Token=strtok(NULL," \t\n\r,");
			if(Token != NULL)
				StartH=atol(Token);
		}
		else if(!stricmp(Token,"/c"))
		{
			Token=strtok(NULL," \t\n\r,");
			if(Token != NULL)
				CompressionType=atol(Token);
		}
		else if(!stricmp(Token,"/f"))
		{
			FirstFrame=1;
		}
		else if(!stricmp(Token,"/t"))
		{
			Token=strtok(NULL," \t\n\r,");
			if(Token != NULL)
				TransColor=atol(Token);
		}
		Token=strtok(NULL," \t\n\r,");
	}

	return(1);
}

// Compression Types
#define PREV_FRAME				1
#define PREV_NEXT_FRAME		2
#define RLE_ONLY				3
#define SPECIAL_FRAME      4

void DeltaImage(char *Image,char *Cmp1,long w,long h,long BytesPerPixel);
long Compress16BitImage(WORD *Src,WORD *Dest,long StartX,long StartY,long NewWidth,long NewHeight,long w,long h);

int PASCAL WinMain ( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR  lpCmdLine, int nCmdShow)
{
	HANDLE ifp,ofp;
	long Size,OutputSize,PrevFrame,NextFrame,i,j,k;
	DWORD bytesread;
	char *WorkImage,*OutputBuf;
	ANIMATION *AnimData;
	ANIMATION OutHeader;

	if(!ParseCommandLine (lpCmdLine))
	{
		printf("Usage: DELTA <Input> <Output> [OPTIONS]\n\n");
		printf("    <Input>          Input Animation file (Generated by MakeAnim.exe)\n");
		printf("    <Output>         Output file (Compressed Anim file\n");
		printf("  [OPTIONS]:\n");
		printf("    /x n             Specify new starting column (Default is 0)\n");
		printf("    /y n             Specify new starting Row (Default is 0)\n");
		printf("    /w n             Specify new width (Default is Image width)\n");
		printf("    /h n             Specify new height (Default is Image height)\n");
		printf("    /t n             Specify transparent color\n");
		printf("    /c n             Compression type (Default 1):\n");
		printf("                        1= Delta & RLE compress with previous frame\n");
		printf("                        2= Delta & RLE compress with previous and next frame\n");
		printf("                        3= RLE compress only NO Delta\n");
 		printf("                        4= Delta & RLE compress with frame 0 (frame 0 not included in output\n");
		printf("    /f               Keep entire 1st frame (Default Don't Keep)\n");
		return(0);
	}

	ifp=CreateFile(AnimFile,GENERIC_READ,FILE_SHARE_READ,NULL,
			   OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,
			   NULL);

	if(ifp == INVALID_HANDLE_VALUE)
	{
		printf("Can't open (%s) [Specified in param file]\n",AnimFile);
		return(0);
	}

	Size=GetFileSize(ifp,NULL);
	AnimData=(ANIMATION *)malloc(Size);
	ReadFile(ifp,AnimData,Size,&bytesread,NULL);
	CloseHandle(ifp);

	if(StartW == -1)
		StartW=AnimData->Width;
	if(StartH == -1)
		StartH=AnimData->Height;

	if(StartX + StartW >= AnimData->Width)
		StartW=AnimData->Width;

	if(StartY + StartH >= AnimData->Width)
		StartH=AnimData->Height;


	if(AnimData->Compression)
	{
		printf("This Animation file already has compression in it (Sorry)\n");
		free(AnimData);
		return(0);
	}

	if(AnimData->BytesPerPixel != 2)
	{
		printf("This program ONLY handles 16bit images (right now)\n");
		free(AnimData);
		return(0);
	}

	ofp=CreateFile(OutputFile,GENERIC_WRITE,FILE_SHARE_WRITE,NULL,
				   CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,
				   NULL);

	memcpy((char *)&OutHeader,(char *)AnimData,sizeof(ANIMATION));
	if(CompressionType == SPECIAL_FRAME)
		OutHeader.Frames=AnimData->Frames-1;
	OutHeader.Compression=CompressionType;
	OutHeader.Width=StartW;
	OutHeader.Height=StartH;

	WriteFile(ofp,&OutHeader,sizeof(ANIMATION),&bytesread,NULL);

	// Create Temp buffer
	Size=AnimData->Width * AnimData->Height * AnimData->BytesPerPixel;
	WorkImage=(char *)malloc(Size);
	if(WorkImage == NULL)
	{
		printf("Can't allocate WorkImage memory\n");
		CloseHandle(ofp);
		free(AnimData);
		return(0);
	}

	OutputBuf=(char *)malloc(Size*100/90); // Add 10 percent in size (for growing)
	if(OutputBuf == NULL)
	{
		printf("Can't allocate Output Buffer memory\n");
		CloseHandle(ofp);
		free(AnimData);
		free(WorkImage);
		return(0);
	}

	printf("Frames=%1ld\n",AnimData->Frames);
	for(i=0;i<AnimData->Frames;i++)
	{
		printf("+");
		switch(CompressionType)
		{
			case PREV_FRAME:
				PrevFrame=i-1;
				if(PrevFrame < 0)
					PrevFrame=AnimData->Frames-1;

				memcpy(WorkImage,&AnimData->Start[sizeof(long)+(sizeof(long) + Size)*i],Size);
				if(!FirstFrame)
					DeltaImage(WorkImage,
						   &AnimData->Start[sizeof(long)+(sizeof(long) + Size)*PrevFrame],
						   AnimData->Width,
						   AnimData->Height,
						   AnimData->BytesPerPixel);
				FirstFrame=0;
				OutputSize=Compress16BitImage((WORD *)WorkImage,(WORD *)OutputBuf,StartX,StartY,StartW,StartH,AnimData->Width,AnimData->Height);
				WriteFile(ofp,&OutputSize,sizeof(long),&bytesread,NULL);
				WriteFile(ofp,OutputBuf,OutputSize,&bytesread,NULL);
				break;
			case PREV_NEXT_FRAME:
				memcpy(WorkImage,&AnimData->Start[sizeof(long)+(sizeof(long) + Size)*i],Size);
				PrevFrame=i-2;
				if(PrevFrame < 0)
					PrevFrame=AnimData->Frames-1;
				DeltaImage(WorkImage,
						   &AnimData->Start[sizeof(long)+(sizeof(long) + Size)*PrevFrame],
						   AnimData->Width,
						   AnimData->Height,
						   AnimData->BytesPerPixel);
//				PrevFrame=i-1;
//				if(PrevFrame < 0)
//					PrevFrame=AnimData->Frames-1;
//				DeltaImage(WorkImage,
//						   &AnimData->Start[sizeof(long)+(sizeof(long) + Size)*PrevFrame],
//						   AnimData->Width,
//						   AnimData->Height,
//						   AnimData->BytesPerPixel);
				NextFrame=i+1;
				if(NextFrame >= AnimData->Frames)
					NextFrame=0;
				DeltaImage(WorkImage,
						   &AnimData->Start[sizeof(long)+(sizeof(long) + Size)*NextFrame],
						   AnimData->Width,
						   AnimData->Height,
						   AnimData->BytesPerPixel);
//				NextFrame=i+2;
//				if(NextFrame >= AnimData->Frames)
//					NextFrame=0;
//				DeltaImage(WorkImage,
//						   &AnimData->Start[sizeof(long)+(sizeof(long) + Size)*NextFrame],
//						   AnimData->Width,
//						   AnimData->Height,
//						   AnimData->BytesPerPixel);
				OutputSize=Compress16BitImage((WORD *)WorkImage,(WORD *)OutputBuf,StartX,StartY,StartW,StartH,AnimData->Width,AnimData->Height);
				WriteFile(ofp,&OutputSize,sizeof(long),&bytesread,NULL);
				WriteFile(ofp,OutputBuf,OutputSize,&bytesread,NULL);
				break;
			case RLE_ONLY:
				memcpy(WorkImage,&AnimData->Start[sizeof(long)+(sizeof(long) + Size)*i],Size);
				for(k=0;k<AnimData->Height;k++)
				{
					for(j=0;j<AnimData->Width;j++)
					{
						if((AnimData->BytesPerPixel == 1) && 
								(WorkImage[k*AnimData->Width*AnimData->BytesPerPixel+j*AnimData->BytesPerPixel] == TransColor))
						{
							WorkImage[k*AnimData->Width*AnimData->BytesPerPixel+j*AnimData->BytesPerPixel]=0xff;
						}
						else if((AnimData->BytesPerPixel == 2) && 
								(WorkImage[k*AnimData->Width*AnimData->BytesPerPixel+j*AnimData->BytesPerPixel] == (TransColor%256)) && 
								(WorkImage[k*AnimData->Width*AnimData->BytesPerPixel+j*AnimData->BytesPerPixel+1] == (TransColor/256)))
						{
							WorkImage[k*AnimData->Width*AnimData->BytesPerPixel+j*AnimData->BytesPerPixel]=0xff;
							WorkImage[k*AnimData->Width*AnimData->BytesPerPixel+j*AnimData->BytesPerPixel+1]=0xff;
						}
					}
				}
				OutputSize=Compress16BitImage((WORD *)WorkImage,(WORD *)OutputBuf,StartX,StartY,StartW,StartH,AnimData->Width,AnimData->Height);
				WriteFile(ofp,&OutputSize,sizeof(long),&bytesread,NULL);
				WriteFile(ofp,OutputBuf,OutputSize,&bytesread,NULL);
				break;
			case SPECIAL_FRAME:
				if(i)
				{
					memcpy(WorkImage,&AnimData->Start[sizeof(long)+(sizeof(long) + Size)*i],Size);
					DeltaImage(WorkImage,
						   &AnimData->Start[sizeof(long)+(sizeof(long) + Size)*0],
						   AnimData->Width,
						   AnimData->Height,
						   AnimData->BytesPerPixel);
					OutputSize=Compress16BitImage((WORD *)WorkImage,(WORD *)OutputBuf,StartX,StartY,StartW,StartH,AnimData->Width,AnimData->Height);
					WriteFile(ofp,&OutputSize,sizeof(long),&bytesread,NULL);
					WriteFile(ofp,OutputBuf,OutputSize,&bytesread,NULL);
				}
				break;
		}
	}
	printf("\n");
	CloseHandle(ofp);
	free(WorkImage);
	free(OutputBuf);
	free(AnimData);
	return(0);
}

void DeltaImage(char *Image,char *Cmp1,long w,long h,long BytesPerPixel)
{
	long i,j,k,Changed;

	for(i=0;i<h;i++)
	{
		for(j=0;j<w;j++)
		{
			Changed=0;
			if(BytesPerPixel == 1 && Image[i*w*BytesPerPixel+j*BytesPerPixel] == TransColor)
			{
			}
			else if(BytesPerPixel == 2 && Image[i*w*BytesPerPixel+j*BytesPerPixel+1] == TransColor%256 && Image[i*w*BytesPerPixel+j*BytesPerPixel] == TransColor/256)
			{
			}
			else if(Cmp1)
			{
				for(k=0;k<BytesPerPixel;k++)
					if(Image[i*w*BytesPerPixel+j*BytesPerPixel+k] != Cmp1[i*w*BytesPerPixel+j*BytesPerPixel+k])
					{

						Changed=1;
						continue;
					}
			}
			if(!Changed)
				for(k=0;k<BytesPerPixel;k++)
					Image[i*w*BytesPerPixel+j*BytesPerPixel+k]=0xff;
		}
	}
}

long Compress16BitImage(WORD *Src,WORD *Dest,long StartX,long StartY,long NewWidth,long NewHeight,long w,long h)
{
	long Size=0,SkipLine=0;
	long i,j,cnt,runs,start,j1;
	long width,height,w1;
	WORD cur;

	height=NewHeight;

	for(i=NewHeight-1;i >= 0;i--)
	{
		for(j=0;j<NewWidth;j++)
		{
			if(Src[(i + StartY)*w + j + StartX] != RLE_NO_DATA)
			{
				i=-1;
				j=NewWidth;
			}
		}
		if(i != -1)
			height--;
	}

	SkipLine=0;
	for(i=0;i<height;i++)
	{
		j=-1;
		for(j1=0;j1<NewWidth && j;j1++)
			if(Src[(i + StartY) * w + j1 + StartX] != RLE_NO_DATA)
				j=0;
		if(!j)
		{
			if(SkipLine)
			{
				*Dest=RLE_SKIPROW | SkipLine;
				Dest++;
				Size++;
				SkipLine=0;
			}

			start=0;

			while(start < NewWidth)
			{
				j=0;
				while(Src[(i + StartY) * w + start + j + StartX] == RLE_NO_DATA && (start + j) < NewWidth)
					j++;
	
				if(j)
				{
					if((start + j) < NewWidth)
					{
						*Dest=RLE_SKIPCOL | j;
						Dest++;
						Size++;
						start+=j;
					}
					else
						start=NewWidth;
				}
	

				if(start < NewWidth)
				{
					width=0;
					while(Src[(i + StartY) * w + start + StartX + width] != RLE_NO_DATA && (width + start) < NewWidth)
						width++;

					if(width)
					{
						cur=Src[(i + StartY) * w + start + StartX];
						j=1;
						runs=1;
						while(j < width)
						{
							if(Src[(i + StartY) * w + start + j + StartX] == cur)
								runs++;
							else
							{
								if(runs > 2)
								{
									if(runs < j)
									{
										*Dest=(j - runs);
										Dest++;
										Size++;
										for(j1=0;j1 < (j - runs);j1++)
										{
											*Dest=Src[(i + StartY) * w + start + j1 + StartX];
											Dest++;
											Size++;
										}
									}
									*Dest=RLE_REPEAT | runs;
									Dest++;
									Size++;
									*Dest=cur;
									Dest++;
									Size++;
									start+=j;
									width-=j;
									j=0;
								}
								runs=1;
								cur=Src[(i + StartY) * w + start + j + StartX];
							}
							j++;
						}
						if(width > 0)
						{
							if(runs > 2)
							{
								if(runs < j)
								{
									*Dest=(j - runs);
									Dest++;
									Size++;
									for(j1=0;j1 < (j - runs);j1++)
									{
										*Dest=Src[(i + StartY) * w + start + j1 + StartX];
										Dest++;
										Size++;
									}
								}
								*Dest=RLE_REPEAT | runs;
								Dest++;
								Size++;
								*Dest=cur;
								Dest++;
								Size++;
								start+=j;
								width-=j;
								j=0;
							}
							else
							{
								*Dest=j;
								Dest++;
								Size++;
								for(j1=0;j1 < j;j1++)
								{
									*Dest=Src[(i + StartY) * w + start + j1 + StartX];
									Dest++;
									Size++;
								}
								start+=j;
								width-=j;
								j=0;
							}
						}
					}
				}
			}
		}
		SkipLine++;
	}
	*Dest=RLE_END;
	Dest++;
	Size++;

	return(Size*sizeof(WORD));
}