enum RenderCommandType { 
	RENDER_NULL,
	RENDER_GLYPH,
	RENDER_MATRIX
};

typedef struct {
	RenderCommandType type;
	float16 matrix; //NOTE: For the Render_Matrix type

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
				
			//TODO: Block from adding any more glpyhs
			if(r->glyphCount < SIZE_OF_GLYPH_INSTANCE) {
				command->offset = r->glyphCount*SIZE_OF_GLYPH_INSTANCE;
				command->size = 0;
			}
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

static void pushMatrix(Renderer *r, float16 m) {
	RenderCommand *c = getRenderCommand(r, RENDER_MATRIX);

	assert(c->type == RENDER_MATRIX);
	c->matrix = m;
}


static void pushGlyph(Renderer *r, void *textureHandle, float2 pos, float4 color, float4 uv) {
	RenderCommand *c = getRenderCommand(r, RENDER_GLYPH);

	assert(c->type == RENDER_GLYPH);

	if(r->glyphCount < MAX_GLYPH_COUNT) {
		float *data = r->glyphInstanceData + r->glyphCount*SIZE_OF_GLYPH_INSTANCE;

		data[0] = pos.x;
		data[1] = pos.y;
		
		data[2] = color.x;
		data[3] = color.y;
		data[4] = color.z;
		data[5] = color.w;

		data[6] = uv.x;
		data[7] = uv.y;
		data[8] = uv.z;
		data[9] = uv.w;

		r->glyphCount++;
		c->size += SIZE_OF_GLYPH_INSTANCE;
	}
}

