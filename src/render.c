

enum RenderCommandType { 
	RENDER_NULL,
	RENDER_GLYPH,
	RENDER_TEXTURE,
	RENDER_MATRIX,
	RENDER_CLEAR_COLOR_BUFFER,
	RENDER_SET_VIEWPORT,
	RENDER_SET_SHADER
};

#define MAX_TEXTURE_COUNT_PER_DRAW_BATCH 1

//NOTE: Eventually we'll want to change this render command to be tight, that is just the command type, and then a data block after it based on what the command is
//		so we're not using up uneccessary space.


typedef struct {
	RenderCommandType type;

	float16 matrix; //NOTE: For the Render_Matrix type
	float4 color; //NOTE: For the RENDER_CLEAR_COLOR_BUFFER
	float4 viewport; //NOTE: For the RENDER_SET_VIEWPORT
	void *shader; //NOTE: For the RENDER_SET_SHADER

	int textureHandle_count;
	void *texture_handles[MAX_TEXTURE_COUNT_PER_DRAW_BATCH]; //NOTE: Used to send to the shader

	int instanceCount;
	int offset_in_bytes;
	int size_in_bytes;
} RenderCommand;

#define MAX_TEXTURE_COUNT 16384
#define MAX_GLYPH_COUNT 16384
#define MAX_RENDER_COMMAND_COUNT 1028

#define SIZE_OF_GLYPH_INSTANCE_IN_BYTES (sizeof(float)*14)

#define GLYPH_INSTANCE_DATA_TOTAL_SIZE_IN_BYTES MAX_GLYPH_COUNT*SIZE_OF_GLYPH_INSTANCE_IN_BYTES

#define SIZE_OF_TEXTURE_INSTANCE_IN_BYTES (sizeof(float)*14)
#define TEXTURE_INSTANCE_DATA_TOTAL_SIZE_IN_BYTES MAX_TEXTURE_COUNT*SIZE_OF_TEXTURE_INSTANCE_IN_BYTES

typedef struct {
	RenderCommandType currentType;

	int commandCount;
	RenderCommand commands[MAX_RENDER_COMMAND_COUNT];

	//NOTE: Instance data
	int glyphCount;
	u8  glyphInstanceData[GLYPH_INSTANCE_DATA_TOTAL_SIZE_IN_BYTES]; //NOTE: This would be x, y, z, r, g, b, a, u, v, s, t, index


	int textureCount;
	u8  textureInstanceData[TEXTURE_INSTANCE_DATA_TOTAL_SIZE_IN_BYTES]; //NOTE: This would be x, y, z, r, g, b, a, u, v, s, t, index


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

static void render_endCommand(Renderer *r) {
	r->currentType = RENDER_NULL;
}

//NOTE: This assumes that we want to always override commands. We need to buffer them if we do.
static RenderCommand *getRenderCommand(Renderer *r, RenderCommandType type) {
	RenderCommand *command = 0;

	if(r->currentType != type) {
		if(r->commandCount < MAX_RENDER_COMMAND_COUNT) {
			command = &r->commands[r->commandCount++];
			command->type = type;
			command->instanceCount = 0;
			command->size_in_bytes = 0;
			command->textureHandle_count = 0;
			r->currentType = type;
				
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
	render_endCommand(r); //NOTE: So you can put more than one clear color on in a row
	RenderCommand *c = getRenderCommand(r, RENDER_CLEAR_COLOR_BUFFER);

	assert(c->type == RENDER_CLEAR_COLOR_BUFFER);
	c->color = color;
}

static int render_getTextureIndex(RenderCommand *c, void *textureHandle) {

	int index = -1;//NOTE: -1 for unitialized

	//NOTE: Check if it's in the array
	for(int i = 0; i < c->textureHandle_count && i < 0; ++i) {
		void *handle = c->texture_handles[i];

		if(handle == textureHandle) {
			index = i;
			break;
		}
	}

	//NOTE: Add to the array now
	if(index < 0 && c->textureHandle_count < MAX_TEXTURE_COUNT_PER_DRAW_BATCH) {
		index = c->textureHandle_count++;
		c->texture_handles[index] =  textureHandle;

	}

	return index;
}

static void pushGlyph(Renderer *r, void *textureHandle, float3 pos, float2 size, float4 color, float4 uv) {
	RenderCommand *c = getRenderCommand(r, RENDER_GLYPH);
	int textureIndex = render_getTextureIndex(c, textureHandle);

	if(textureIndex < 0) {
		render_endCommand(r);

		RenderCommand *c = getRenderCommand(r, RENDER_GLYPH);
		int textureIndex = render_getTextureIndex(c, textureHandle);
		assert(textureIndex >= 0);
	}

	assert(c->type == RENDER_GLYPH);
	

	if(r->glyphCount < MAX_GLYPH_COUNT) {
		float *data = (float *)(r->glyphInstanceData + r->glyphCount*SIZE_OF_GLYPH_INSTANCE_IN_BYTES);

		data[0] = pos.x;
		data[1] = pos.y;
		data[2] = pos.z;

		data[3] = size.x;
		data[4] = size.y;
		
		data[5] = color.x;
		data[6] = color.y;
		data[7] = color.z;
		data[8] = color.w;

		data[9] = uv.x;
		data[10] = uv.y;
		data[11] = uv.z;
		data[12] = uv.w;

		data[13] = textureIndex;

		r->glyphCount++;
		c->instanceCount++;
		c->size_in_bytes += SIZE_OF_GLYPH_INSTANCE_IN_BYTES;
	}
}


static void pushTexture(Renderer *r, void *textureHandle, float3 pos, float2 size, float4 color, float4 uv) {
	RenderCommand *c = getRenderCommand(r, RENDER_TEXTURE);
	int textureIndex = render_getTextureIndex(c, textureHandle);

	if(textureIndex < 0) {
		render_endCommand(r);

		RenderCommand *c = getRenderCommand(r, RENDER_GLYPH);
		int textureIndex = render_getTextureIndex(c, textureHandle);
		assert(textureIndex >= 0);
	}

	assert(c->type == RENDER_TEXTURE);
	

	if(r->textureCount < MAX_TEXTURE_COUNT) {
		float *data = (float *)(r->textureInstanceData + r->textureCount*SIZE_OF_TEXTURE_INSTANCE_IN_BYTES);

		data[0] = pos.x;
		data[1] = pos.y;
		data[2] = pos.z;

		data[3] = size.x;
		data[4] = size.y;
		
		data[5] = color.x;
		data[6] = color.y;
		data[7] = color.z;
		data[8] = color.w;

		data[9] = uv.x;
		data[10] = uv.y;
		data[11] = uv.z;
		data[12] = uv.w;

		data[13] = textureIndex;

		r->textureCount++;
		c->instanceCount++;
		c->size_in_bytes += SIZE_OF_TEXTURE_INSTANCE_IN_BYTES;
	}
}

