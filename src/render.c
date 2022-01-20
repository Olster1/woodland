enum RenderCommandType { 
	RENDER_NULL,
	RENDER_GLYPH,
	RENDER_MATRIX,
	RENDER_CLEAR_COLOR_BUFFER,
	RENDER_SET_VIEWPORT,
	RENDER_SET_SHADER
};

//NOTE: Eventually we'll want to change this render command to be tight, that is just the command type, and then a data block after it based on what the command is
//		so we're not using up uneccessary space.

typedef struct {
	RenderCommandType type;

	float16 matrix; //NOTE: For the Render_Matrix type
	float4 color; //NOTE: For the RENDER_CLEAR_COLOR_BUFFER
	float4 viewport; //NOTE: For the RENDER_SET_VIEWPORT
	void *shader; //NOTE: For the RENDER_SET_SHADER

	int instanceCount;
	int offset_in_bytes;
	int size_in_bytes;
} RenderCommand;

#define MAX_GLYPH_COUNT 16384
#define MAX_RENDER_COMMAND_COUNT 1028

#define SIZE_OF_GLYPH_INSTANCE_IN_BYTES (sizeof(float)*10)

#define GLYPH_INSTANCE_DATA_TOTAL_SIZE_IN_BYTES MAX_GLYPH_COUNT*SIZE_OF_GLYPH_INSTANCE_IN_BYTES

typedef struct {
	RenderCommandType currentType;

	int commandCount;
	RenderCommand commands[MAX_RENDER_COMMAND_COUNT];

	//NOTE: Instance data
	int glyphCount;
	u8  glyphInstanceData[GLYPH_INSTANCE_DATA_TOTAL_SIZE_IN_BYTES]; //NOTE: This would be x, y, r, g, b, a, u, v, s, t


} Renderer;

static void initRenderer(Renderer *r) {
	r->commandCount = 0;
	r->currentType = RENDER_NULL;
	r->glyphCount = 0;
}

static void clearRenderer(Renderer *r) {
	r->commandCount = 0;
	r->currentType = RENDER_NULL;
	r->glyphCount = 0;	
}

static RenderCommand *getRenderCommand(Renderer *r, RenderCommandType type) {
	RenderCommand *command = 0;

	if(r->currentType != type) {
		if(r->commandCount < MAX_RENDER_COMMAND_COUNT) {
			command = &r->commands[r->commandCount++];
			command->type = type;
			command->instanceCount = 0;
			command->size_in_bytes = 0;
				
			//TODO: Block from adding any more glpyhs
			if(r->glyphCount < MAX_GLYPH_COUNT) {
				command->offset_in_bytes = r->glyphCount*SIZE_OF_GLYPH_INSTANCE_IN_BYTES;
				
			} else {
				assert(!"glyph data buffer full");
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

static void pushShader(Renderer *r, void *shader) {
	RenderCommand *c = getRenderCommand(r, RENDER_SET_SHADER);

	assert(c->type == RENDER_SET_SHADER);
	c->shader = shader;
}

static void pushViewport(Renderer *r, float4 v) {
	RenderCommand *c = getRenderCommand(r, RENDER_SET_VIEWPORT);

	assert(c->type == RENDER_SET_VIEWPORT);
	c->viewport = v;
}


static void pushClearColor(Renderer *r, float4 color) {
	RenderCommand *c = getRenderCommand(r, RENDER_CLEAR_COLOR_BUFFER);

	assert(c->type == RENDER_CLEAR_COLOR_BUFFER);
	c->color = color;
}

static void pushGlyph(Renderer *r, void *textureHandle, float2 pos, float4 color, float4 uv) {
	RenderCommand *c = getRenderCommand(r, RENDER_GLYPH);

	assert(c->type == RENDER_GLYPH);

	if(r->glyphCount < MAX_GLYPH_COUNT) {
		float *data = (float *)(r->glyphInstanceData + r->glyphCount*SIZE_OF_GLYPH_INSTANCE_IN_BYTES);

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
		c->instanceCount++;
		c->size_in_bytes += SIZE_OF_GLYPH_INSTANCE_IN_BYTES;
	}
}

