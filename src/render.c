enum RenderCommandType { 
	RENDER_NULL,
	RENDER_GLYPH,
	RENDER_MATRIX
};

typedef struct {
	RenderCommandType type;

	float16 orthoMatrix;

	int offset;
	int size;
} RenderCommand;

#define MAX_GLYPH_COUNT 16384
#define MAX_RENDER_COMMAND_COUNT 1028

#define SIZE_OF_GLYPH_INSTANCE (sizeof(float)*10)

typedef struct {
	RenderCommandType currentType;

	int commandCount;
	RenderCommand commands[MAX_RENDER_COMMAND_COUNT];

	//NOTE: Instance data
	int glyphCount;
	float glyphInstanceData[MAX_GLYPH_COUNT*SIZE_OF_GLYPH_INSTANCE]; //NOTE: This would be x, y, r, g, b, a, u, v, s, t

} Renderer;

static void initRenderer(Renderer *r) {
	r->commandCount = 0;
	r->currentType = RENDER_NULL;
	r->glyphCount = 0;
}

static RenderCommand *getRenderCommand(Renderer *r, RenderCommandType type) {
	RenderCommand *command = 0;

	if(r->currentType != type) {
		if(r->commandCount < MAX_RENDER_COMMAND_COUNT) {
			command = &r->commands[r->commandCount++];

			if(r->glyphCount < SIZE_OF_GLYPH_INSTANCE)
			command->offset = r->glyphCount*SIZE_OF_GLYPH_INSTANCE;
			command->size = 0;
		} else {
			assert(!"Command buffer full");
		}

	} else {
		assert(r->commandCount > 0);
		command = &r->commands[r->commandCount - 1];
	}

	assert(r->commandCount > 0);

	return command;
}

static void pushGlyph(Renderer *r, float2 pos, float4 color, float4 uv) {
	RenderCommand *c = getRenderCommand(r, RENDER_GLYPH);
}

static void drawRenderBuffer(Renderer *r) {

	for(int i = 0; i < r->commandCount; ++i) {
		RenderCommand *c = r->commands + i;

		switch(c->type) {
			case RENDER_NULL: {
				assert(false);
			} break;
			case RENDER_GLYPH: {
				

				
			} break;
			case RENDER_MATRIX: {

			} break;
		}
	}
}