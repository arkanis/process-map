#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <elf.h>


typedef struct {
	void* elf_mmap_start;
	size_t elf_mmap_size;
	
	struct { const char* name; size_t fileOffset; size_t size; }* at;
	size_t count;
} ElfSymbols;

ElfSymbols LoadElfSymbols(const char* pathToElfFile);
void       UnloadElfSymbols(ElfSymbols* symbols);

int main(int argc, char** argv) {
	if (argc != 2) {
		fprintf(stderr, "usage: %s path-to-elf-file\n", argv[0]);
		return 1;
	}
	
	ElfSymbols symbols = LoadElfSymbols(argv[1]);
	for (size_t i = 0; i < symbols.count; i++)
		printf("at 0x%08zx %4lu bytes: %s\n", symbols.at[i].fileOffset, symbols.at[i].size, symbols.at[i].name);
	UnloadElfSymbols(&symbols);
	
	return 0;
}

ElfSymbols LoadElfSymbols(const char* pathToElfFile) {
	ElfSymbols symbols = (ElfSymbols){ 0 };
	struct stat stats;
	if ( stat(pathToElfFile, &stats) != 0 )
		return symbols;
	if ( !S_ISREG(stats.st_mode) )  // abort if file isn't a regular file
		return symbols;
	
	int fd = open(pathToElfFile, O_RDONLY);
	void* mmap_start = mmap(NULL, stats.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
	close(fd);
	if (mmap_start == MAP_FAILED)
		return symbols;
	
	symbols.elf_mmap_start = mmap_start;
	symbols.elf_mmap_size = stats.st_size;
	
	Elf64_Ehdr* header = symbols.elf_mmap_start;
	if ( !(header->e_ident[0] == ELFMAG0 && header->e_ident[1] == ELFMAG1 && header->e_ident[2] == ELFMAG2 && header->e_ident[3] == ELFMAG3) ) {
		UnloadElfSymbols(&symbols);
		return symbols;
	}
	
	// Search for the .dynsym (dynamic linking symbols) and .dynstr (string table for symbol names) sections
	Elf64_Shdr *dynsym = NULL, *dynstr = NULL;
	Elf64_Shdr* section_name_strtab = symbols.elf_mmap_start + header->e_shoff + header->e_shstrndx * header->e_shentsize;
	const char* section_names = symbols.elf_mmap_start + section_name_strtab->sh_offset;
	for (size_t i = 1; i < header->e_shnum; i++) {  // Note: Index 0 is reserved, skip it
		Elf64_Shdr* section = symbols.elf_mmap_start + header->e_shoff + i * header->e_shentsize;
		const char* section_name = section_names + section->sh_name;
		if ( strcmp(section_name, ".dynsym") == 0 && section->sh_type == SHT_DYNSYM ) {
			dynsym = section;
		} else if ( strcmp(section_name, ".dynstr") == 0 && section->sh_type == SHT_STRTAB ) {
			dynstr = section;
		}
	}
	
	if (dynsym == NULL || dynstr == NULL) {
		UnloadElfSymbols(&symbols);
		return symbols;
	}
	
	const char* symbol_names = symbols.elf_mmap_start + dynstr->sh_offset;
	for (size_t offset = 0; offset < dynsym->sh_size; offset += dynsym->sh_entsize) {
		Elf64_Sym* symbol = symbols.elf_mmap_start + dynsym->sh_offset + offset;
		
		// We're only interested in symbols that are defined in a section of this file (have a shndx) and contain actual code (a size and type STT_FUNC)
		if ( symbol->st_shndx != SHN_UNDEF && symbol->st_size > 0 && ELF64_ST_TYPE(symbol->st_info) == STT_FUNC ) {
			Elf64_Shdr* code_section = symbols.elf_mmap_start + header->e_shoff + symbol->st_shndx * header->e_shentsize;
			
			symbols.count++;
			symbols.at = realloc(symbols.at, sizeof(symbols.at[0]) * symbols.count);
			symbols.at[symbols.count - 1].name = symbol_names + symbol->st_name;
			symbols.at[symbols.count - 1].fileOffset = code_section->sh_offset + symbol->st_value;
			symbols.at[symbols.count - 1].size = symbol->st_size;
		}
	}
	
	return symbols;
}

void UnloadElfSymbols(ElfSymbols* symbols) {
	munmap(symbols->elf_mmap_start, symbols->elf_mmap_size);
	free(symbols->at);
	*symbols = (ElfSymbols){ 0 };
}