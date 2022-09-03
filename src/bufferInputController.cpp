/*
This file represents the controller that adds text to a WL_Buffer. 
Copy, Paste, write text, move cursor around

*/



static bool optionCanMoveUp(BufferControllerOption option) {
    if(option == BUFFER_ALL) {
        return true;
    } else if(option == BUFFER_SIMPLE) {
        return false;
    } else {
        return true;
    }
}

static bool optionCanMoveDown(BufferControllerOption option) {
    if(option == BUFFER_ALL) {
        return true;
    } else if(option == BUFFER_SIMPLE) {
        return false;
    } else {
        return true;
    }
}

static void updateNewCursorPos(EditorState *editorState, WL_Buffer *b, int new_cursor_pos_inBytes, Selectable_State *selectable_state) {
	if(new_cursor_pos_inBytes >= 0){ //is valid move
		if(global_platformInput.keyStates[PLATFORM_KEY_SHIFT].isDown) 
		{
			update_select(selectable_state, b->cursorAt_inBytes);
		} else {
			end_select(selectable_state);
		}

	    b->cursorAt_inBytes = new_cursor_pos_inBytes;

	    if(global_platformInput.keyStates[PLATFORM_KEY_SHIFT].isDown) 
	    {
	    	assert(selectable_state->is_active);
	    	update_select(selectable_state, b->cursorAt_inBytes);
	    } else {
			end_select(selectable_state);
		}
	}
}

static int getCursorPosAtStartOfLine(EditorState *editorState, WL_Buffer *b) {
	u8 *start = (u8 *)(b->bufferMemory + (b->cursorAt_inBytes)); 
	u8 *at = start;

	if(*at == '\n' && (at - 1) >= b->bufferMemory) { at--; } //NOTE: Move past new line
	if(*at == '\r' && (at - 1) >= b->bufferMemory) { at--; } //NOTE: Move past new line

	//NOTE: Walk backwards till you find a new line
	while(at >= b->bufferMemory) {
		if(*at == '\n' || *at == '\r' || at == b->bufferMemory) {
			//NOTE: Found new line
			if(*at == '\n' || *at == '\r') {at++;}
			break;
		}
		at--;
	}

	int result = (int)(at - b->bufferMemory);
	return result;

}

static int getCursorPosAtEndOfLine(EditorState *editorState, WL_Buffer *b) {
	u8 *start = (u8 *)(b->bufferMemory + (b->cursorAt_inBytes)); 
	u8 *at = start;

	//NOTE: Walk backwards till you find a new line
	while(at < (b->bufferMemory + b->bufferSize_inUse_inBytes)) {
		if(*at == '\n' || *at == '\r') {
			//NOTE: Found new line
			break;
		}
		at++;
	}

	int result = (int)(at - b->bufferMemory);
	return result;
}

static float getXposAtInLine(EditorState *editorState, WL_Buffer *b, Font *font, float fontScale) {
	u8 *start = (u8 *)(b->bufferMemory + (b->cursorAt_inBytes)); 
	u8 *at = start;

	//NOTE: Walk backwards till you find a new line
	while(at >= b->bufferMemory) {
		if(*at == '\n' || *at == '\r' || at == b->bufferMemory) {
			//NOTE: Found new line
			break;
		}
		at--;
	}

	if(*at == '\n' || *at == '\r') {
		at++;
	}

	float xAt = 0;
	//Now walk forwards to get x posistion
	while(*at != '\n' && *at != '\r' && at < (b->bufferMemory + b->bufferSize_inUse_inBytes) && at < (b->bufferMemory + b->cursorAt_inBytes)) {
		u32 rune = easyUnicode_utf8_codepoint_To_Utf32_codepoint(&((char *)at), true);
		float factor = 1.0f;

		if(rune == '\t') {
			rune = (u32)' ';
			factor = 4.0f;
		} 

		GlyphInfo g = easyFont_getGlyph(font, rune);	
		assert(g.unicodePoint == rune);

		xAt += (g.width + g.xoffset)*fontScale*factor;
	}


	return xAt;
}

static int getCusorPosLineBelow(EditorState *editorState, WL_Buffer *b, Font *font, float fontScale, WL_Open_Buffer *open_buffer) {
	int new_cursor_pos_inBytes = -1; //-1 not valid move

	u8 *at =  b->bufferMemory + b->cursorAt_inBytes;

	bool found = false;
	while(at < (b->bufferMemory + b->bufferSize_inUse_inBytes) && !found) {
		//NOTE: look for two lines above

		if(at == (b->bufferMemory + b->bufferSize_inUse_inBytes - 1)) {
			//NOTE: We are one the first line of the buffer so don't bother trying to move down, so new_cursor_pos_inBytes is invalid 
			found = true;
			break;
		}

		if((*at == '\n' || *at == '\r') && !found) {

			//NOTE: Found new line
			found = true;

			if(*at == '\r') { at++; } //NOTE: Move past new line
			if(*at == '\n') { at++; } //NOTE: Move past new line

			bool looking_for = true;

			if(*at == '\r' || *at == '\n') {
				//NOTE: we are at a new line which got missed in the loop below, so we just set the new cursor position here
				new_cursor_pos_inBytes = at - b->bufferMemory;
				looking_for = false;

			}


			
			float xAt = 0; 
			float bestPos = FLT_MAX;

			//NOTE: Walk until end of line or best_position is found
			while(*at != '\n' && *at != '\r' && at < (b->bufferMemory + b->bufferSize_inUse_inBytes) && looking_for) {
				u32 rune = easyUnicode_utf8_codepoint_To_Utf32_codepoint(&((char *)at), true);
				float factor = 1.0f;

				if(rune == '\t') {
					rune = (u32)' ';
					factor = 4.0f;
				} 

				GlyphInfo g = easyFont_getGlyph(font, rune);	
				assert(g.unicodePoint == rune);

				xAt += (g.width + g.xoffset)*fontScale*factor;

				float val = get_abs_value(xAt - open_buffer->moveVertical_xPos);
				if(val < bestPos) {
					bestPos = val;
					new_cursor_pos_inBytes = at - b->bufferMemory;
				} else {
					looking_for = false;						
				}

				//NOTE: at incremented by the getglyph function
			}
		}

		at++;
	}
	return new_cursor_pos_inBytes;
}

static int getCusorPosLineAbove(EditorState *editorState, WL_Buffer *b, Font *font, float fontScale, WL_Open_Buffer *open_buffer) {
	int new_cursor_pos_inBytes = -1; //-1 not valid move

	u8 *at =  b->bufferMemory + b->cursorAt_inBytes;

	//NOTE: Because we're moving backwards we could be at the end of line and the cursor would be at a newline, so this would cound as a new line, so we move back from these
	if(*at == '\n' && (at - 1) >= b->bufferMemory) { at--; } //NOTE: Move past new line
	if(*at == '\r' && (at - 1) >= b->bufferMemory) { at--; } //NOTE: Move past new line

	int newlineCount = 0;
	bool found = false;
	while(at >= b->bufferMemory && !found) {
		//NOTE: look for two lines above

		if(at == b->bufferMemory && newlineCount == 0) {
			//NOTE: We are one the first line of the buffer so don't bother trying to move up
			found = true;
			break;
		}

		if(*at == '\n' || *at == '\r' || (newlineCount == 1 && at == b->bufferMemory)) {

			
			if(newlineCount == 0) {
				if((at - 1) >= b->bufferMemory && *at == '\n' && *(at - 1) == '\r') {
					at--; //Move past the newline 'token'because two characters cound as one new line :(
				}
			}
			//NOTE: Found new line
			newlineCount++;

			if(newlineCount > 1) {

				//NOTE: Found new line
				found = true;

				if(*at == '\r') { at++; } //NOTE: Move past new line
				if(*at == '\n') { at++; } //NOTE: Move past new line

				bool looking_for = true;

				if(*at == '\r' || *at == '\n') {
					//NOTE: we are at a new line which got missed in the loop below, so we just set the new cursor position here
					new_cursor_pos_inBytes = at - b->bufferMemory;
					looking_for = false;

				}

				float xAt = 0; 
				float bestPos = FLT_MAX;

				//NOTE: Walk until end of line or best_position is found
				while(looking_for && *at != '\n' && *at != '\r' && at < (b->bufferMemory + b->bufferSize_inUse_inBytes)) {
					u32 rune = easyUnicode_utf8_codepoint_To_Utf32_codepoint(&((char *)at), true);
					float factor = 1.0f;

					if(rune == '\t') {
						rune = (u32)' ';
						factor = 4.0f;
					} 

					GlyphInfo g = easyFont_getGlyph(font, rune);	
					assert(g.unicodePoint == rune);

					xAt += (g.width + g.xoffset)*fontScale*factor;

					float val = get_abs_value(xAt - open_buffer->moveVertical_xPos);
					if(val < bestPos) {
						bestPos = val;
						new_cursor_pos_inBytes = at - b->bufferMemory;
					} else {
						looking_for = false;						
					}

					//NOTE: at incremented by the getglyph function
				}
			}
		}

		at--;
	}

	return new_cursor_pos_inBytes;
}

static void process_buffer_controller(EditorState *editorState, WL_Open_Buffer *open_buffer, WL_Buffer *b, BufferControllerOption option, Selectable_State *selectable_state) {
    //NOTE: Ctrl B -> open buffer chooser
    if(global_platformInput.keyStates[PLATFORM_KEY_CTRL].isDown && global_platformInput.keyStates[PLATFORM_KEY_B].pressedCount > 0) 
    {
        if(editorState->mode_ == MODE_BUFFER_SELECT){
            set_editor_mode(editorState, MODE_EDIT_BUFFER);
        } else {
            set_editor_mode(editorState, MODE_BUFFER_SELECT);
        }
        
        editorState->ui_state.use_mouse = true;

    }

    //NOTE: Ctrl F -> open buffer chooser
    if(global_platformInput.keyStates[PLATFORM_KEY_CTRL].isDown && global_platformInput.keyStates[PLATFORM_KEY_F].pressedCount > 0) 
    {
        if(editorState->mode_ == MODE_FIND){
            set_editor_mode(editorState, MODE_EDIT_BUFFER);
        } else {
            set_editor_mode(editorState, MODE_FIND);
        }
        
        editorState->ui_state.use_mouse = true;

    }

    //NOTE: Ctrl V -> paste text from clipboard
    if(global_platformInput.keyStates[PLATFORM_KEY_CTRL].isDown && global_platformInput.keyStates[PLATFORM_KEY_V].pressedCount > 0) 
    {
        char *text_from_clipboard = platform_get_text_utf8_from_clipboard(&globalPerFrameArena);
          
        //NOTE: Any text added
        addTextToBuffer(b, (char *)text_from_clipboard, b->cursorAt_inBytes);

        if(open_buffer) {
            open_buffer->is_up_to_date = false;
            open_buffer->should_scroll_to = true;
        }  

        end_select(selectable_state);

    }

    //NOTE: UNDO REDO commands 
    if(option == BUFFER_ALL) {
        UndoRedoBlock *block = NULL;
        bool isRedo = true;

        //NOTE: Ctrl Z -> copy text to clipboard
        if(global_platformInput.keyStates[PLATFORM_KEY_CTRL].isDown && global_platformInput.keyStates[PLATFORM_KEY_Z].pressedCount > 0) 
        {   
            isRedo = false;
            block = get_undo_block(&b->undo_redo_state);
        }

        //NOTE: Ctrl Y -> copy text to clipboard
        if(global_platformInput.keyStates[PLATFORM_KEY_CTRL].isDown && global_platformInput.keyStates[PLATFORM_KEY_Y].pressedCount > 0) 
        {
            block = get_redo_block(&b->undo_redo_state);
        }

        if(block) {

            block->byteAt;
            block->string;
            block->stringLength;

            bool shouldInsert = true;

            UndoRedo_BlockType commandType = block->type;

            if(!isRedo) {
                //NOTE: Reverse the command on the buffer block
                if(commandType == UNDO_REDO_INSERT) {
                    commandType = UNDO_REDO_DELETE;
                } else {
                    assert(commandType == UNDO_REDO_DELETE);
                    commandType = UNDO_REDO_INSERT;
                }
            } 

            if(open_buffer) {
                open_buffer->moveVertical_xPos = -1;

                assert(block->id > 0);

                u32 id_to_check = (isRedo) ? block->id : (block->id -1);

                //NOTE: Check if it is up to date based on the id we saved at
                if(open_buffer->current_save_undo_redo_id == id_to_check) {
                    open_buffer->is_up_to_date = true;
                } else {
                    open_buffer->is_up_to_date = false;
                }
                
                open_buffer->should_scroll_to = true;
                end_select(selectable_state);
            }   

            if(block->byteAt != b->cursorAt_inBytes) {
                endGapBuffer(b);
            }

            if(commandType == UNDO_REDO_INSERT) { //NOTE: Insert
                
                addTextToBuffer(b, block->string, block->byteAt, false);
            } else {
                assert(commandType == UNDO_REDO_DELETE); //NOTE: Delete
            
                removeTextFromBuffer(b, block->byteAt, block->stringLength, false);

            }

            open_buffer->should_scroll_to = true;
        }
        
    }

    //NOTE: Ctrl C -> copy text to clipboard
    if(global_platformInput.keyStates[PLATFORM_KEY_CTRL].isDown && global_platformInput.keyStates[PLATFORM_KEY_C].pressedCount > 0 && selectable_state->is_active) 
    {
        Selectable_Diff diff = selectable_get_bytes_diff(selectable_state);
        platform_copy_text_utf8_to_clipboard((char *)(((u8 *)b->bufferMemory) + diff.start), diff.size);

    }


    //NOTE: Any text added if not pressing ctrl
    if(global_platformInput.textInput_utf8[0] != '\0' && !global_platformInput.keyStates[PLATFORM_KEY_CTRL].isDown) {
        if(option == BUFFER_SIMPLE) {
            //NOTE: Don't add new lines to SIMPLE buffers
            char *at = (char *)global_platformInput.textInput_utf8;
            while(*at) {
                if(*at == '\n' || *at == '\r') {
                    //NOTE: Move everything down one byte
                    char *temp = at;
                    while(*temp) {
                        temp[0] = temp[1];
                        temp++;
                    }
                } else {
                    at++;
                }
                
            }
            
        }
        addTextToBuffer(b, (char *)global_platformInput.textInput_utf8, b->cursorAt_inBytes);

        if(open_buffer) {
            open_buffer->is_up_to_date = false;
            open_buffer->should_scroll_to = true;
        }  
        end_select(selectable_state);
    }

    // OutputDebugStringA((char *)global_platformInput.textInput_utf8);

    //NOTE: Process our command buffer
    for(int i = 0; i < global_platformInput.keyInputCommand_count; ++i) {
        PlatformKeyType command = global_platformInput.keyInputCommandBuffer[i];

        if(open_buffer) {
            open_buffer->should_scroll_to = true;
        }

        if(command == PLATFORM_KEY_BACKSPACE && b->cursorAt_inBytes > 0) {
            if(open_buffer) {
                open_buffer->moveVertical_xPos = -1;
            }

            u32 bytesOfPrevRune = size_of_last_utf8_codepoint_in_bytes((char *)&b->bufferMemory[b->cursorAt_inBytes], b->cursorAt_inBytes);
            removeTextFromBuffer(b, b->cursorAt_inBytes - bytesOfPrevRune, bytesOfPrevRune);
           

            if(open_buffer) {
                open_buffer->is_up_to_date = false;
            }

            end_select(selectable_state);
            
        }

        if(command == PLATFORM_KEY_LEFT) {
            
            endGapBuffer(b);

            if(open_buffer) {
                open_buffer->moveVertical_xPos = -1;
            }
            
            u32 bytesOfPrevRune = size_of_last_utf8_codepoint_in_bytes((char *)&b->bufferMemory[b->cursorAt_inBytes], b->cursorAt_inBytes);

            if(global_platformInput.keyStates[PLATFORM_KEY_CTRL].isDown) {
                EasyToken token = peekTokenBackwards_tokenNotComplete((char *)(b->bufferMemory + (b->cursorAt_inBytes - 1)), (char *)b->bufferMemory);
            
                if(token.type != TOKEN_UNINITIALISED) {
                    bytesOfPrevRune = token.size;
                }
            }

            //NOTE: Move cursor left 
            if(b->cursorAt_inBytes > 0) {

                //NOTE: Windows style newline
                if(b->cursorAt_inBytes >= 2 && b->bufferMemory[b->cursorAt_inBytes - 2] == '\r' && b->bufferMemory[b->cursorAt_inBytes - 1] == '\n') {
                    bytesOfPrevRune = 2;
                }

                if(global_platformInput.keyStates[PLATFORM_KEY_SHIFT].isDown) 
                {
                    update_select(selectable_state, b->cursorAt_inBytes);
                } else {
                    end_select(selectable_state);
                }
                

                b->cursorAt_inBytes -= bytesOfPrevRune;

                if(global_platformInput.keyStates[PLATFORM_KEY_SHIFT].isDown) 
                {
                    assert(selectable_state->is_active);
                    update_select(selectable_state, b->cursorAt_inBytes);
                } else {
                    end_select(selectable_state);
                }

            }
        }

        if(command == PLATFORM_KEY_RIGHT) {

            u32 bytesOfNextRune = size_of_next_utf8_codepoint_in_bytes((char *)&b->bufferMemory[b->cursorAt_inBytes]);

            endGapBuffer(b);

            if(open_buffer) {
                open_buffer->moveVertical_xPos = -1;
            }

            if(global_platformInput.keyStates[PLATFORM_KEY_CTRL].isDown) {

                EasyToken token = peekTokenForward_tokenNotComplete((char *)(b->bufferMemory + (b->cursorAt_inBytes)), (char *)(b->bufferMemory + (b->bufferSize_inUse_inBytes)));

                if(token.type != TOKEN_UNINITIALISED) {
                    bytesOfNextRune = token.size;
                }
            }
            
            
            //NOTE: Move cursor right 
            if(b->cursorAt_inBytes < b->bufferSize_inUse_inBytes) {

                //NOTE: Windows style newline
                if(b->cursorAt_inBytes < (b->bufferSize_inUse_inBytes - 1) && b->bufferMemory[b->cursorAt_inBytes] == '\r' && b->bufferMemory[b->cursorAt_inBytes + 1] == '\n') {
                    bytesOfNextRune = 2;
                }

                if(global_platformInput.keyStates[PLATFORM_KEY_SHIFT].isDown) 
                {

                    update_select(selectable_state, b->cursorAt_inBytes);
                } else {
                    end_select(selectable_state);
                }

                b->cursorAt_inBytes += bytesOfNextRune;

                if(global_platformInput.keyStates[PLATFORM_KEY_SHIFT].isDown) 
                {
                    assert(selectable_state->is_active);
                    update_select(selectable_state, b->cursorAt_inBytes);
                } else {
                    end_select(selectable_state);
                }
            }
        }   

        if(command == PLATFORM_KEY_END) {

            endGapBuffer(b);

            
            int new_cursor_pos_inBytes = getCursorPosAtEndOfLine(editorState, b);

            
            updateNewCursorPos(editorState, b, new_cursor_pos_inBytes, selectable_state);
        }  

        if(command == PLATFORM_KEY_HOME) {

            endGapBuffer(b);
            int new_cursor_pos_inBytes = getCursorPosAtStartOfLine(editorState, b);
            updateNewCursorPos(editorState, b, new_cursor_pos_inBytes, selectable_state);
            
            
        }  

        if(command == PLATFORM_KEY_UP && optionCanMoveUp(option)) { //NOTE: Some buffer controllers you can't move up 

            assert(open_buffer); //NOTE: Must have an open buffer to fo this option

            endGapBuffer(b);

            if(editorState->moveVertical_xPos < 0) {
                editorState->moveVertical_xPos = getXposAtInLine(editorState, b, &editorState->font, editorState->fontScale);
            }

            assert(editorState->moveVertical_xPos  >= 0);
            int new_cursor_pos_inBytes = getCusorPosLineAbove(editorState, b, &editorState->font, editorState->fontScale, open_buffer);
            
            updateNewCursorPos(editorState, b, new_cursor_pos_inBytes, selectable_state);
        }  

        if(command == PLATFORM_KEY_DOWN && optionCanMoveDown(option)) {
            assert(open_buffer);//NOTE: Must have an open buffer to do this option
            endGapBuffer(b);

            if(editorState->moveVertical_xPos < 0) {
                editorState->moveVertical_xPos = getXposAtInLine(editorState, b, &editorState->font, editorState->fontScale);
            }

            assert(editorState->moveVertical_xPos  >= 0);
            int new_cursor_pos_inBytes = getCusorPosLineBelow(editorState, b, &editorState->font, editorState->fontScale, open_buffer);

            updateNewCursorPos(editorState, b, new_cursor_pos_inBytes, selectable_state);				   	
        }      
    }
}