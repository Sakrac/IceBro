#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"
#include <stdint.h>
int main(int argc, char* argv[])
{
	if (argc < 3) {
		printf("Usage: IconsToCPP image symbol");
		return 0;
	}
	int x, y, n;
	uint8_t *data = stbi_load(argv[1], &x, &y, &n, 0);
	if(data) {
		printf("// Converted from \"%s\"\n", argv[1]);
		printf("extern const int %s_Width = %d;\nextern const int %s_Height = %d;\nextern const int %s_Comp = %d;\n", argv[2], x, argv[2], y, argv[2], n);
		int bytes = x * y * n;
		printf("extern const unsigned char %s_Pixels[%d] = {\n\t", argv[2], bytes);
		for (int p = 0; p < bytes; ++p) {
			printf("0x%02x", data[p]);
			if ((p + 1) != bytes) { printf(", "); }
			if ((p % 20) == 19) { printf("\n\t"); }
		}
		printf(" };\n\n");
		stbi_image_free(data);
	}
	return 0;
}
