ID3D11Buffer* global_vertexBuffer_quad;
ID3D11DeviceContext1* d3d11DeviceContext;

static void backendRender_processCommandBuffer(Renderer *r) {

	for(int i = 0; i < r->commandCount; ++i) {
		RenderCommand *c = r->commands + i;

		switch(c->type) {
			case RENDER_NULL: {
				assert(false);
			} break;
			case RENDER_GLYPH: {

				float *data = r->glyphInstanceData + c->offset;
				int sizeInBytes = c->size;
				


				// d3d11DeviceContext->VSSetConstantBuffers(0, 1, &constantBuffer);
				// d3d11DeviceContext->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
				// d3d11DeviceContext->Draw(numVerts, 0);
				
			} break;
			case RENDER_MATRIX: {

			} break;
		}
	}
}