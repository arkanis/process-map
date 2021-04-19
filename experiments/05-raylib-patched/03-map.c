#define _GNU_SOURCE  // for asprintf(), basename(), strdup()
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <elf.h>

#include "raylib.h"


typedef struct {
	void* elfMmapStart;
	size_t elfMmapSize;
	
	struct { const char* name; size_t fileOffset; size_t size; }* at;
	size_t count;
} ElfSymbols;

ElfSymbols LoadElfSymbols(const char* path);
void       UnloadElfSymbols(ElfSymbols* symbols);

typedef struct {
	char* text;
	float x, y;
	float fontSize;
} RangeLabel;

typedef struct {
	uint64_t startAddr, endAddr;
	bool isGap;
	char* perms;
	char* filePath;
	uint64_t fileOffset;
	
	uint64_t startPage, endPage;
	uint32_t color;
	Rectangle boundingBox;
	char* label;
	int labelX, labelY;
	float labelFontSize;
	
	Texture2D symbolMap;
	size_t symbolCount;
	RangeLabel* symbolLables;
} AddrRange;

void      LoadAddrRanges(char* pid, AddrRange** ranges, size_t* rangeCount);
void      UnloadAddrRanges(AddrRange** ranges, size_t* rangeCount);

Texture2D PlotAddrRanges(AddrRange ranges[], size_t rangeCount, Font rangeNameFont, size_t* hilbertCurveOrder);
uint64_t  HilberCurvePosToDist(uint64_t curveOrder, int x, int y);
void      HilberCurveDistToPos(uint64_t curveOrder, uint64_t dist, int* x, int* y);
uint32_t  fnv1a(void *buffer, size_t size);

int main(int argc, char** argv) {
	if (argc != 2) {
		fprintf(stderr, "usage: %s pid\n", argv[0]);
		return 0;
	}
	
	FILE* cmdLineFile = fopen(TextFormat("/proc/%s/cmdline", argv[1]), "rb");
	if (cmdLineFile == NULL) {
		fprintf(stderr, "Couldn't find process %s\n", argv[1]);
		return 0;
	}
	char* cmdLine = NULL;
	fscanf(cmdLineFile, "%m[^\n]", &cmdLine);
	fclose(cmdLineFile);
	
	const int screenWidth = 800, screenHeight = 600;
	InitWindow(screenWidth, screenHeight, TextFormat("Memory map of process %s: %s", argv[1], cmdLine));
	
    uint32_t fontFileSize = 0;
    uint8_t* fontFileData = LoadFileData("NotoSans-Regular.ttf", &fontFileSize);
    Font fontSDF = { .baseSize = 32, .charsCount = 95 };
    fontSDF.chars = LoadFontData(fontFileData, fontFileSize, fontSDF.baseSize, NULL, 0, FONT_SDF);
    Image atlas = GenImageFontAtlas(fontSDF.chars, &fontSDF.recs, 95, fontSDF.baseSize, 0, 1);
    fontSDF.texture = LoadTextureFromImage(atlas);
    SetTextureFilter(fontSDF.texture, FILTER_BILINEAR);
    UnloadImage(atlas);
    UnloadFileData(fontFileData);
    Shader fontShader = LoadShader(NULL, "sdf-text.fs");
    
	
	AddrRange* ranges = NULL;
	size_t rangeCount = 0;
	LoadAddrRanges(argv[1], &ranges, &rangeCount);
	size_t hilbertCurveOrder = 0;
	Texture2D mapTexture = PlotAddrRanges(ranges, rangeCount, fontSDF, &hilbertCurveOrder);
	//for (size_t i = 0; i < rangeCount; i++)
	//	printf("%3zu: color 0x%08x %s\n", i, ranges[i].color, ranges[i].label);
	
	Shader mapShader = LoadShader(NULL, "range-map.fs");
	int selectedColorLoc = GetShaderLocation(mapShader, "selectedColor");
	Shader symbolMapShader = LoadShader(NULL, "symbol-map.fs");
	
	Camera2D camera = (Camera2D){
		.target = (Vector2){ mapTexture.width / 2, mapTexture.height / 2 },
		.offset = (Vector2){ screenWidth / 2, screenHeight / 2 },
		.zoom = 1.0
	};
	
	bool wasWindowFocused = true, isPanning = false;
	Vector2 prevMousePos = { 0 };
	while ( !WindowShouldClose() ) {
		// Reduce FPS when window isn't focused
		if ( IsWindowFocused() && !wasWindowFocused ) {
			wasWindowFocused = true;
			SetTargetFPS(0);
		} else if ( !IsWindowFocused() && wasWindowFocused ) {
			wasWindowFocused = false;
			SetTargetFPS(30);
		}
		
		// Mouse panning
		if ( IsMouseButtonPressed(MOUSE_LEFT_BUTTON) ) {
			isPanning = true;
			prevMousePos = GetMousePosition();
		} else if ( IsMouseButtonReleased(MOUSE_LEFT_BUTTON) ) {
			isPanning = false;
		} else if ( isPanning ) {
			float dx = prevMousePos.x - GetMouseX(), dy = prevMousePos.y - GetMouseY();
			camera.target.x += dx / camera.zoom;
			camera.target.y += dy / camera.zoom;
			prevMousePos = GetMousePosition();
		}
		
		// Mousewheel zooming (world space position remains the same directly at the mouse pointer)
		const float factor = powf(0.9, -GetMouseWheelMove());  // 0.9^1 = 0.9, 0.9^-1 = 1/0.9
		Vector2 mousePosWs = GetScreenToWorld2D(GetMousePosition(), camera);
		float oldScale = camera.zoom, newScale = camera.zoom * factor;
		camera.target.x = mousePosWs.x + (camera.target.x - mousePosWs.x) * (oldScale / newScale);
		camera.target.y = mousePosWs.y + (camera.target.y - mousePosWs.y) * (oldScale / newScale);
		camera.zoom = newScale;
		
		// Handle keyboard input
		if (IsKeyPressed(KEY_R)) {
			mapShader = LoadShader(NULL, "range-map.fs");
			symbolMapShader = LoadShader(NULL, "symbol-map.fs");
		}
		
		// Picking
		uint32_t mapSelectedColor = -1;
		ssize_t  mapSelectedRangeIndex = -1;
		if ( CheckCollisionPointRec(mousePosWs, (Rectangle){ .x = 0, .y = 0, .width = mapTexture.width, .height = mapTexture.height }) ) {
			uint64_t page = HilberCurvePosToDist(hilbertCurveOrder, mousePosWs.x, mousePosWs.y);
			for (size_t i = 0; i < rangeCount; i++) {
				if (page >= ranges[i].startPage && page < ranges[i].endPage) {
					mapSelectedColor = ranges[i].color;
					mapSelectedRangeIndex = i;
					break;
				}
			}
		}
		
		
		// Prepare view culling
		Vector2 viewTopLeft = GetScreenToWorld2D((Vector2){ 0, 0 }, camera);
		Vector2 viewBottomRight = GetScreenToWorld2D((Vector2){ screenWidth, screenHeight }, camera);
		Rectangle viewBoundingBox = (Rectangle){ .x = viewTopLeft.x, .y = viewTopLeft.y, .width = viewBottomRight.x - viewTopLeft.x, .height = viewBottomRight.y - viewTopLeft.y };
		
		BeginDrawing();
			ClearBackground(WHITE);
			
			BeginMode2D(camera);
				BeginShaderMode(mapShader);
					Vector4 selectedColor = ColorNormalize(*((Color*)&mapSelectedColor));
					SetShaderValue(mapShader, selectedColorLoc, &selectedColor, UNIFORM_VEC4);
					DrawTexture(mapTexture, 0, 0, BLANK);
				EndShaderMode();
				
				if (mapSelectedRangeIndex != -1) {
					Rectangle rect = ranges[mapSelectedRangeIndex].boundingBox;
					DrawRectangleLines(rect.x, rect.y, rect.width, rect.height, (Color){ 0, 255, 0, 64 });
					BeginShaderMode(fontShader);
						DrawTextEx(fontSDF, ranges[mapSelectedRangeIndex].label, (Vector2){ ranges[mapSelectedRangeIndex].labelX, ranges[mapSelectedRangeIndex].labelY }, ranges[mapSelectedRangeIndex].labelFontSize, 0, BLACK);
					EndShaderMode();
				}
				
				
				for (size_t i = 0; i < rangeCount; i++) {
					if ( CheckCollisionRecs(ranges[i].boundingBox, viewBoundingBox) ) {
						BeginShaderMode(fontShader);
							DrawTextEx(fontSDF, ranges[i].label, (Vector2){ ranges[i].labelX, ranges[i].labelY }, ranges[i].labelFontSize, 0, (Color){ 0, 0, 0, 128 });
						EndShaderMode();
						
						if (ranges[i].boundingBox.width > viewBoundingBox.width / 4) {
							BeginShaderMode(symbolMapShader);
								DrawTexturePro(ranges[i].symbolMap, (Rectangle){ 0, 0, ranges[i].symbolMap.width, ranges[i].symbolMap.height }, ranges[i].boundingBox, (Vector2){ 0, 0 }, 0, (Color){0,0,0,64});
							EndShaderMode();
							
							BeginShaderMode(fontShader);
								for (size_t s = 0; s < ranges[i].symbolCount; s++) {
									RangeLabel* label = &ranges[i].symbolLables[s];
									DrawTextEx(fontSDF, label->text, (Vector2){ label->x, label->y }, label->fontSize, 0, (Color){ 0, 0, 0, 128 });
								}
							EndShaderMode();
						}
					}
				}
				
			EndMode2D();
		EndDrawing();
	}
	
	UnloadShader(mapShader);
	UnloadTexture(mapTexture);
	UnloadAddrRanges(&ranges, &rangeCount);
	UnloadShader(fontShader);
	UnloadFont(fontSDF);
	UnloadShader(symbolMapShader);
	CloseWindow();
	free(cmdLine);
	
	return 0;
}



// 
// Loading address ranges of a process
// 

void LoadAddrRanges(char* pid, AddrRange** ranges, size_t* rangeCount) {
	AddrRange* list = NULL;
	size_t count = 0;
	
	char* mapsPath = NULL;
	asprintf(&mapsPath, "/proc/%s/maps", pid);
	FILE* mapsFile = fopen(mapsPath, "rb");
	
	uint64_t prevRangeEnd = 0;
	while ( !feof(mapsFile) ) {
		uint64_t from, to, offset;
		char *perms = NULL, *pathname = NULL;
		int items_read = fscanf(mapsFile, "%lx-%lx%*[ ]%ms%*[ ]%lx%*[ ]%*s%*[ ]%*s%*[ ]%m[^\n]", &from, &to, &perms, &offset, &pathname);
		if (items_read < 3)
			continue;
		
		uint64_t gapSize = from - prevRangeEnd;
		if (gapSize > 0) {
			count++;
			list = realloc(list, sizeof(list[0]) * count);
			list[count - 1] = (AddrRange){ .startAddr = from, .endAddr = to, .isGap = true };
		}
		
		count++;
		list = realloc(list, sizeof(list[0]) * count);
		list[count - 1] = (AddrRange){ .startAddr = from, .endAddr = to, .isGap = false, .perms = perms, .filePath = pathname, .fileOffset = offset };
		
		prevRangeEnd = to;
	}
	count++;
	list = realloc(list, sizeof(list[0]) * count);
	list[count - 1] = (AddrRange){ .startAddr = prevRangeEnd, .endAddr = 0xffffffffffffffff, .isGap = true };
	
	fclose(mapsFile);
	
	*ranges = list;
	*rangeCount = count;
}

void UnloadAddrRanges(AddrRange** ranges, size_t* rangeCount) {
	for (size_t i = 0; i < *rangeCount; i++) {
		AddrRange* range = &(*ranges)[i];
		free(range->perms);
		free(range->filePath);
		free(range->label);
		UnloadTexture(range->symbolMap);
		free(range->symbolLables);
	}
	free(*ranges);
	*ranges = NULL;
	*rangeCount = 0;
}



// 
// Plotting address ranges onto a 2D map
// 

Texture2D PlotAddrRanges(AddrRange ranges[], size_t rangeCount, Font rangeNameFont, size_t* hilbertCurveOrder) {
	const size_t pageSize = 4096, cappedGapSize = 16 * 1024*1024;
	
	// First figure out how many pages we need to fit into the map, from that we can calculate the size of the map square.
	uint64_t minRequiredPages = 0, oversizedGapPages = 0;
	for (size_t i = 0; i < rangeCount; i++) {
		size_t rangePageCount = (ranges[i].endAddr - ranges[i].startAddr) / pageSize;
		if (ranges[i].isGap) {
			if (rangePageCount > cappedGapSize) {
				minRequiredPages += cappedGapSize;
				oversizedGapPages += rangePageCount;
			} else {
				minRequiredPages += rangePageCount;
			}
		} else {
			minRequiredPages += rangePageCount;
		}
	}
	
	// Caclulate texture size and the order of the hilbert curve used to map the 1D address ranges onto a 2D plane.
	// sqrt(minRequiredPages) is the size of the square needed to fit minRequiredPages pixels.
	// log2(squareSize) is the order of the hilber curve (and the power-of-two exponent of the square size). It needs to be an integer so we round up with ceil().
	*hilbertCurveOrder = ceil(log2(sqrt(minRequiredPages)));
	size_t mapSideLength = pow(2, *hilbertCurveOrder);
	
	// We now know the size of the map, distribute the left over map space among the oversized gaps (proportinally to their respective relative size)
	uint64_t leftOverFreePages = mapSideLength*mapSideLength - minRequiredPages;
	uint64_t prevRangeEndPage = 0;
	for (size_t i = 0; i < rangeCount; i++) {
		size_t rangePageCount = (ranges[i].endAddr - ranges[i].startAddr) / pageSize;
		ranges[i].startPage = prevRangeEndPage;
		// Broken right now?
		/*if (i == rangeCount - 1) {
			// Make sure the end of the last range aligns with the end of the map
			ranges[i].endPage = mapSideLength*mapSideLength;
		} else*/ if (ranges[i].isGap && rangePageCount > cappedGapSize) {
			rangePageCount += leftOverFreePages * ((double)rangePageCount / oversizedGapPages);
			ranges[i].endPage = ranges[i].startPage + rangePageCount;
			// Round the endPage towards to the closest 16M boundary to avoid jagged edges on the map 
			ranges[i].endPage = (ranges[i].endPage + cappedGapSize / 2) / cappedGapSize * cappedGapSize;
		} else {
			ranges[i].endPage = ranges[i].startPage + rangePageCount;
		}
		
		prevRangeEndPage = ranges[i].endPage;
	}
	
	// Create a map image where the address ranges are mapped into the image via a hilbert curve.
	// Each range gets its own color: RG are random bits from the hashed filePath, BA are bits of the range index.
	// While we iterate over each pixel of a range also calculate the bounding box for later (text positioning, draw culling, etc.).
	Image mapImage = GenImageColor(mapSideLength, mapSideLength, (Color){ 1, 2, 3, 255 });
	uint32_t* mapImagePixels = mapImage.data;
	for (size_t i = 0; i < rangeCount; i++) {
		uint32_t color = 0;
		if (ranges[i].isGap)
			color = (i & 0x0000ffff);
		else if (ranges[i].filePath)
			color = (fnv1a(ranges[i].filePath, strlen(ranges[i].filePath)) & 0xffff0000) | (i & 0x0000ffff);
		else
			color = (fnv1a(&ranges[i], offsetof(AddrRange, isGap)) << 16) | (i & 0x0000ffff);
		color = __builtin_bswap32(color);
		
		int xMin = mapSideLength, xMax = 0, yMin = mapSideLength, yMax = 0;
		for (uint64_t page = ranges[i].startPage; page < ranges[i].endPage; page++) {
			int x, y;
			HilberCurveDistToPos(*hilbertCurveOrder, page, &x, &y);
			mapImagePixels[y * mapImage.width + x] = color;
			
			if (x < xMin) xMin = x;
			if (x > xMax) xMax = x;
			if (y < yMin) yMin = y;
			if (y > yMax) yMax = y;
		}
		
		ranges[i].color = color;
		ranges[i].boundingBox = (Rectangle){ .x = xMin, .y = yMin, .width = xMax - xMin + 1, .height = yMax - yMin + 1 };
	}
	
	// Create the label text for each region and calculate the labels position and font size
	for (size_t i = 0; i < rangeCount; i++) {
		char* label = NULL;
		if (ranges[i].isGap)
			label = strdup("unmapped");
		else if (ranges[i].filePath)
			label = strdup(basename(ranges[i].filePath));
		else
			label = strdup("anonymous");
		
		Vector2 labelSize = MeasureTextEx(rangeNameFont, label, 10, 0);
		Rectangle boundingBox = ranges[i].boundingBox;
		float labelFontScale = (boundingBox.width * 0.66) / labelSize.x;
		
		ranges[i].label = label;
		ranges[i].labelFontSize = 10 * labelFontScale;
		ranges[i].labelX = boundingBox.x + (boundingBox.width - labelSize.x * labelFontScale) / 2;
		ranges[i].labelY = boundingBox.y + (boundingBox.height - labelSize.y * labelFontScale) / 2;
	}

	// Load the ELF symbols for each region. If there are any 
	char* prevRangeFilePath = NULL;
	ElfSymbols prevRangeSymbols = { 0 };
	for (size_t i = 0; i < rangeCount; i++) {
		AddrRange* range = &ranges[i];
		// Reuse the ELF symbols of the previous range if the filePath is the same.
		// Just some cheap caching to avoid loading the same ELF for successive ranges.
		ElfSymbols symbols = { 0 };
		if ( range->filePath && prevRangeFilePath && strcmp(range->filePath, prevRangeFilePath) == 0 ) {
			symbols = prevRangeSymbols;
		} else {
			symbols = LoadElfSymbols(range->filePath);
			UnloadElfSymbols(&prevRangeSymbols);
			prevRangeSymbols = symbols;
		}
		
		if (symbols.count == 0)
			continue;
		
		printf("%3zu: color 0x%08x, %zu symbols %s\n", i, range->color, symbols.count, range->label);
		
		size_t symbolMapHilberCurveOrder = *hilbertCurveOrder + 3;
		size_t symbolMapScale = pow(2, 3);
		//Image symbolMapImage = GenImageChecked(range->boundingBox.width * symbolMapScale, range->boundingBox.height * symbolMapScale, 1, 1, BLACK, WHITE);
		Image symbolMapImage = GenImageColor(range->boundingBox.width * symbolMapScale, range->boundingBox.height * symbolMapScale, BLANK);
		uint32_t* symbolMapImagePixels = symbolMapImage.data;
		
		size_t pixelsPerPage = symbolMapScale * symbolMapScale;
		size_t bytesPerPixel = pageSize / pixelsPerPage;
		uint64_t rangeStartPixel = range->startPage * pixelsPerPage;
		
		for (size_t s = 0; s < symbols.count; s++) {
			uint64_t rangeStartOffset = range->fileOffset, rangeEndOffset = range->fileOffset + (range->endAddr - range->startAddr);
			if ( !(symbols.at[s].fileOffset >= rangeStartOffset && symbols.at[s].fileOffset < rangeEndOffset) )
				continue;
			
			// Symbol is in the file area mapped into this address range
			uint64_t symbolStartPixel = rangeStartPixel + (symbols.at[s].fileOffset - rangeStartOffset) / bytesPerPixel;
			uint64_t symbolEndPixel = symbolStartPixel + symbols.at[s].size / bytesPerPixel;
			//printf("  symbol %s\n", symbols.at[s].name);
			
			float xMin = mapSideLength * symbolMapScale, xMax = 0, yMin = mapSideLength * symbolMapScale, yMax = 0;
			for (uint64_t pixel = symbolStartPixel; pixel < symbolEndPixel; pixel++) {
				int globalX, globalY;
				HilberCurveDistToPos(symbolMapHilberCurveOrder, pixel, &globalX, &globalY);
				int x = globalX - range->boundingBox.x * symbolMapScale;
				int y = globalY - range->boundingBox.y * symbolMapScale;
				
				//printf("    global %d %d local %d %d\n", globalX, globalY, x, y);
				symbolMapImagePixels[y * symbolMapImage.width + x] = (s + 1) * 16;
				
				if (globalX < xMin) xMin = globalX;
				if (globalX > xMax) xMax = globalX;
				if (globalY < yMin) yMin = globalY;
				if (globalY > yMax) yMax = globalY;
			}
			
			Rectangle symbolBoundingBox = (Rectangle){
				.x = xMin / symbolMapScale, .y = yMin / symbolMapScale,
				.width = (xMax - xMin + 1) / symbolMapScale,
				.height = (yMax - yMin + 1) / symbolMapScale
			};
			
			Vector2 labelSize = MeasureTextEx(rangeNameFont, symbols.at[s].name, 10, 0);
			float labelFontScale = (symbolBoundingBox.width * 0.66) / labelSize.x;
			
			range->symbolCount++;
			range->symbolLables = realloc(range->symbolLables, range->symbolCount * sizeof(range->symbolLables[0]));
			range->symbolLables[range->symbolCount - 1].text = strdup(symbols.at[s].name);
			range->symbolLables[range->symbolCount - 1].fontSize = 10 * labelFontScale;
			range->symbolLables[range->symbolCount - 1].x = symbolBoundingBox.x + (symbolBoundingBox.width - labelSize.x * labelFontScale) / 2;
			range->symbolLables[range->symbolCount - 1].y = symbolBoundingBox.y + (symbolBoundingBox.height - labelSize.y * labelFontScale) / 2;
		}
		
		range->symbolMap = LoadTextureFromImage(symbolMapImage);
	}
	UnloadElfSymbols(&prevRangeSymbols);
	
	Texture2D mapTexture = LoadTextureFromImage(mapImage);
	UnloadImage(mapImage);
	return mapTexture;
}

uint64_t HilberCurvePosToDist(uint64_t curveOrder, int x, int y) {
	uint64_t d = 0, tmp;
	for (uint64_t s = (1 << curveOrder) / 2; s > 0; s /= 2) {
		uint64_t rx = 0, ry = 0;
		
		if ((x & s) > 0) rx = 1;
		if ((y & s) > 0) ry = 1;
		
		d += s * s * ((3 * rx) ^ ry);
		
		// inlining
		// hilbertRot(s, p, rx, ry)
		if (ry == 0) {
			if (rx == 1) {
				x = s - 1 - x;
				y = s - 1 - y;
			}
			tmp = x;
			x = y;
			y = tmp;
		}
		// end inline
	}
	return d;
}

void HilberCurveDistToPos(uint64_t curveOrder, uint64_t dist, int* x, int* y) {
	*x = 0; *y = 0;
	uint64_t tmp;
	uint64_t n = 1 << curveOrder;
	for (uint64_t s = 1; s < n; s *= 2) {
		uint64_t rx = 1 & (dist / 2);
		uint64_t ry = 1 & (dist ^ rx);
		// inlining
		// hilbertRot(s, p, rx, ry)
		if (ry == 0) {
			if (rx == 1) {
				*x = s - 1 - *x;
				*y = s - 1 - *y;
			}
			tmp = *x;
			*x = *y;
			*y = tmp;
		}
		// end inline

		*x += s * rx;
		*y += s * ry;
		dist /= 4;
	}
}

/*
uint32_t fnv1a(const char* string) {
	uint32_t hash = 2166136261;
	for(char* c = string; *c != '\0'; c++) {
		hash ^= *c;
		hash *= 16777619;
	}
	return hash;
}
*/

// Based on http://www.isthe.com/chongo/tech/comp/fnv/
uint32_t fnv1a(void *buffer, size_t size) {
	uint32_t hash = 2166136261;
	for (char* c = buffer; c < (char*)buffer + size; c++) {
		hash ^= *c;
		hash *= 16777619;
	}
	return hash;
}





// 
// ELF symbol loading
// 

ElfSymbols LoadElfSymbols(const char* path) {
	ElfSymbols symbols = (ElfSymbols){ 0 };
	struct stat stats;
	if ( stat(path, &stats) != 0 )  // abort if file doesn't exist, e.g. "[heap]"
		return symbols;
	if ( !S_ISREG(stats.st_mode) )  // abort if file isn't a regular file
		return symbols;
	
	int fd = open(path, O_RDONLY);
	void* mmapStart = mmap(NULL, stats.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	close(fd);
	if (mmapStart == MAP_FAILED)
		return symbols;
	
	symbols.elfMmapStart = mmapStart;
	symbols.elfMmapSize = stats.st_size;
	
	Elf64_Ehdr* header = symbols.elfMmapStart;
	if ( !(header->e_ident[0] == ELFMAG0 && header->e_ident[1] == ELFMAG1 && header->e_ident[2] == ELFMAG2 && header->e_ident[3] == ELFMAG3) ) {
		UnloadElfSymbols(&symbols);
		return symbols;
	}
	
	// Search for the .dynsym (dynamic linking symbols) and .dynstr (string table for symbol names) sections
	Elf64_Shdr *dynsym = NULL, *dynstr = NULL;
	Elf64_Shdr* sectionNameStrtab = symbols.elfMmapStart + header->e_shoff + header->e_shstrndx * header->e_shentsize;
	const char* sectionNames = symbols.elfMmapStart + sectionNameStrtab->sh_offset;
	for (size_t i = 1; i < header->e_shnum; i++) {  // Note: Index 0 is reserved, skip it
		Elf64_Shdr* section = symbols.elfMmapStart + header->e_shoff + i * header->e_shentsize;
		const char* sectionName = sectionNames + section->sh_name;
		if ( strcmp(sectionName, ".dynsym") == 0 && section->sh_type == SHT_DYNSYM ) {
			dynsym = section;
		} else if ( strcmp(sectionName, ".dynstr") == 0 && section->sh_type == SHT_STRTAB ) {
			dynstr = section;
		}
	}
	
	if (dynsym == NULL || dynstr == NULL) {
		UnloadElfSymbols(&symbols);
		return symbols;
	}
	
	const char* symbolNames = symbols.elfMmapStart + dynstr->sh_offset;
	for (size_t offset = 0; offset < dynsym->sh_size; offset += dynsym->sh_entsize) {
		Elf64_Sym* symbol = symbols.elfMmapStart + dynsym->sh_offset + offset;
		
		// We're only interested in symbols that are defined in a section of this file (have a shndx) and contain actual code (a size and type STT_FUNC)
		if ( symbol->st_shndx != SHN_UNDEF && symbol->st_size > 0 && ELF64_ST_TYPE(symbol->st_info) == STT_FUNC ) {
			//Elf64_Shdr* codeSection = symbols.elfMmapStart + header->e_shoff + symbol->st_shndx * header->e_shentsize;
			
			symbols.count++;
			symbols.at = realloc(symbols.at, sizeof(symbols.at[0]) * symbols.count);
			symbols.at[symbols.count - 1].name = symbolNames + symbol->st_name;
			symbols.at[symbols.count - 1].fileOffset = /*codeSection->sh_offset + */symbol->st_value;  // Looks like symbol offsets are global file offsets
			symbols.at[symbols.count - 1].size = symbol->st_size;
		}
	}
	
	return symbols;
}

void UnloadElfSymbols(ElfSymbols* symbols) {
	munmap(symbols->elfMmapStart, symbols->elfMmapSize);
	free(symbols->at);
	*symbols = (ElfSymbols){ 0 };
}