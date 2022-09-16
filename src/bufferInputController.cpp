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

static void updateNewCursorPos(WL_Buffer *b, int new_cursor_pos_inBytes, Selectable_State *selectable_state) {
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

static int getCursorPosAtStartOfLine(WL_Buffer *b) {
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

static int getCursorPosAtEndOfLine(WL_Buffer *b) {
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

static float getXposAtInLine(WL_Buffer *b, Font *font, float fontScale) {
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

static int getCusorPosLineBelow(WL_Buffer *b, Font *font, float fontScale, WL_Open_Buffer *open_buffer) {
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

//NOTE: helper function to remove text if it is highlighted
static void remove_text_if_highlighted(Selectable_State *selectable_state, WL_Buffer *b) {
    if(selectable_state->is_active) {
        Selectable_Diff diff = selectable_get_bytes_diff(selectable_state);
        removeTextFromBuffer(b, diff.start, diff.size);

        end_select(selectable_state);
    }
}

static void process_buffer_controller(EditorState *editorState, WL_Open_Buffer *open_buffer, WL_Buffer *b, BufferControllerOption option, Selectable_State *selectable_state, bool justNumber = false) { //NOTE: Just number could be change to flags if we have more options
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

      //NOTE: Ctrl G -> jump to line
    if(global_platformInput.keyStates[PLATFORM_KEY_CTRL].isDown && global_platformInput.keyStates[PLATFORM_KEY_G].pressedCount > 0) 
    {
         if(editorState->mode_ == MODE_GO_TO_LINE){
            set_editor_mode(editorState, MODE_EDIT_BUFFER);
        } else {
            set_editor_mode(editorState, MODE_GO_TO_LINE);
        }
        
    }

    //NOTE: Ctrl F -> open buffer chooser
    if(global_platformInput.keyStates[PLATFORM_KEY_CTRL].isDown && global_platformInput.keyStates[PLATFORM_KEY_F].pressedCount > 0) 
    {
        if(editorState->mode_ == MODE_FIND){
            set_editor_mode(editorState, MODE_EDIT_BUFFER);
        } else {
            set_editor_mode(editorState, MODE_FIND);
        }
        
        //NOTE: Prefernce to mouse movement
        editorState->ui_state.use_mouse = true;

    }

    //NOTE: Ctrl A -> select all
    if(global_platformInput.keyStates[PLATFORM_KEY_CTRL].isDown && global_platformInput.keyStates[PLATFORM_KEY_A].pressedCount > 0) 
    {
        end_select(selectable_state);

        update_select(&open_buffer->selectable_state, 0);
        update_select(&open_buffer->selectable_state, b->bufferSize_inUse_inBytes);

        b->cursorAt_inBytes = b->bufferSize_inUse_inBytes;

        open_buffer->should_scroll_to = true;

    }

    //NOTE: Any text added if not pressing ctrl
    if(global_platformInput.textInput_utf8[0] != '\0' && !global_platformInput.keyStates[PLATFORM_KEY_CTRL].isDown) {

        if(open_buffer) {
            open_buffer->cursor_blink_time = 0.0f;
        }
        
        if(option == BUFFER_SIMPLE) {
            //NOTE: Don't add new lines to SIMPLE buffers
            char *at = (char *)global_platformInput.textInput_utf8;
            while(*at) {
                assert(easyUnicode_isSingleByte(*at));

                //NOTE: If justNumber is set check that this is just numbers
                bool numberCheck = (lexIsNumeric(*at) && justNumber) || !justNumber; 

                if(*at == '\n' || *at == '\r' || !numberCheck) {
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

        remove_text_if_highlighted(selectable_state, b);

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
            open_buffer->cursor_blink_time = 0.0f;
        }

         //NOTE: Ctrl X -> cut text to clipboard
        if(global_platformInput.keyStates[PLATFORM_KEY_CTRL].isDown && command == PLATFORM_KEY_X) 
        {   
            if(selectable_state->is_active && selectable_state->start_offset_in_bytes < selectable_state->end_offset_in_bytes) {
                Selectable_Diff diff = selectable_get_bytes_diff(selectable_state);
                platform_copy_text_utf8_to_clipboard((char *)(((u8 *)b->bufferMemory) + diff.start), diff.size);

                remove_text_if_highlighted(selectable_state, b);

                end_select(selectable_state);

            } else {
                endGapBuffer(b);

                //NOTE: cut whole line
                size_t new_cursor_pos_inBytes_start = b->cursorAt_inBytes;

                u8 *start = (u8 *)(b->bufferMemory + (b->cursorAt_inBytes)); 

                while(start >= b->bufferMemory && start[0] != '\n') {
                    new_cursor_pos_inBytes_start--;
                    start--;
                }

                //NOTE: Move past the newline because we don't want to take it out
                if(start[0] == '\n') { new_cursor_pos_inBytes_start++; }

                size_t new_cursor_pos_inBytes_end = b->cursorAt_inBytes;

                start = (u8 *)(b->bufferMemory + (b->cursorAt_inBytes)); 

                while(start[0] != '\0' && start[0] != '\n') {
                    new_cursor_pos_inBytes_end++;
                    start++;
                }

                if(start[0] == '\n') { new_cursor_pos_inBytes_end++; }


                removeTextFromBuffer(b, new_cursor_pos_inBytes_start, new_cursor_pos_inBytes_end - new_cursor_pos_inBytes_start);
            }
        }

        //NOTE: Ctrl C -> cut text to clipboard
        if(global_platformInput.keyStates[PLATFORM_KEY_CTRL].isDown  && command == PLATFORM_KEY_C) 
        {
            if(selectable_state->is_active) {
                Selectable_Diff diff = selectable_get_bytes_diff(selectable_state);
                platform_copy_text_utf8_to_clipboard((char *)(((u8 *)b->bufferMemory) + diff.start), diff.size);

            } 
        }

            //NOTE: Ctrl V -> paste text from clipboard
        if(global_platformInput.keyStates[PLATFORM_KEY_CTRL].isDown && command == PLATFORM_KEY_V) 
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

            //NOTE: Loop this if undo commands are in a group
            bool hasBlocks = true;
            s32 startGroupId = -1;
            while(hasBlocks) {

                UndoRedoBlock *block = NULL;
                bool isRedo = true;

                //NOTE: Ctrl Z -> undo operation
                if(global_platformInput.keyStates[PLATFORM_KEY_CTRL].isDown && command == PLATFORM_KEY_Z) 
                {   
                    isRedo = false;
                    UndoRedoBlock *nextBlock = see_undo_block(&b->undo_redo_state);

                    //NOTE: If there are any blocks to get, is part of this group or is not in a group or starting a group
                    if(nextBlock && (nextBlock->groupId == startGroupId || nextBlock->groupId < 0 || (nextBlock->groupId >= 0 && startGroupId < 0))) {
                        block = get_undo_block(&b->undo_redo_state);
                    }
                    
                }

                //NOTE: Ctrl Y -> redo operation
                if(global_platformInput.keyStates[PLATFORM_KEY_CTRL].isDown && command == PLATFORM_KEY_Y) 
                {
                    UndoRedoBlock *nextBlock = see_redo_block(&b->undo_redo_state);

                    if(nextBlock && (nextBlock->groupId == startGroupId || nextBlock->groupId < 0 || (nextBlock->groupId >= 0 && startGroupId < 0))) {
                        block = get_redo_block(&b->undo_redo_state);
                    }
                }

                if(block) {

                    if(startGroupId < 0) {
                        //NOTE: Initilise the start group id
                        startGroupId = block->groupId;
                    }

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

                    //NOTE: Check if buffer is in a saved state based on the new undo state 
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

                    //NOTE: See if this block is part of the groupId or is a single command
                    if(block->groupId < 0) {
                        hasBlocks = false;
                    }
                } else {
                    //NOTE: If we don't have any more blocks left, get out of the loop
                    hasBlocks = false;
                }
                open_buffer->should_scroll_to = true;

                //NOTE: For testing
                hasBlocks = false;
            }
        }

        if(command == PLATFORM_KEY_BACKSPACE && b->cursorAt_inBytes > 0) {
            if(open_buffer) {
                //NOTE: de activate the move vertical position so it gets a new one next time
                open_buffer->moveVertical_xPos = -1;
            }
            size_t startByte = 0;
            size_t totalBytes = 0;
            if(selectable_state->is_active) {
                remove_text_if_highlighted(selectable_state, b);
            } else {
                u32 bytesOfPrevRune = size_of_last_utf8_codepoint_in_bytes((char *)&b->bufferMemory[b->cursorAt_inBytes], b->cursorAt_inBytes);
                assert(bytesOfPrevRune == 1 || bytesOfPrevRune == 0);
                
                startByte = b->cursorAt_inBytes - bytesOfPrevRune;
                totalBytes = bytesOfPrevRune;
                removeTextFromBuffer(b, startByte, totalBytes);
            }

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
            
            assert(bytesOfPrevRune == 1 || bytesOfPrevRune == 0);

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
            assert(bytesOfNextRune == 1 || bytesOfNextRune == 0);
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

            
            int new_cursor_pos_inBytes = getCursorPosAtEndOfLine(b);

            
            updateNewCursorPos(b, new_cursor_pos_inBytes, selectable_state);
        }  

        if(command == PLATFORM_KEY_HOME) {

            endGapBuffer(b);
            int new_cursor_pos_inBytes = getCursorPosAtStartOfLine(b);
            updateNewCursorPos( b, new_cursor_pos_inBytes, selectable_state);
            
            
        }  

        if(command == PLATFORM_KEY_UP && optionCanMoveUp(option)) { //NOTE: Some buffer controllers you can't move up 

            assert(open_buffer); //NOTE: Must have an open buffer to fo this option

            endGapBuffer(b);

            if(open_buffer->moveVertical_xPos < 0) {
                open_buffer->moveVertical_xPos = getXposAtInLine(b, &editorState->font, editorState->fontScale);
            }

            assert(open_buffer->moveVertical_xPos  >= 0);
            int new_cursor_pos_inBytes = getCusorPosLineAbove(editorState, b, &editorState->font, editorState->fontScale, open_buffer);
            
            updateNewCursorPos(b, new_cursor_pos_inBytes, selectable_state);
        }  

        if(command == PLATFORM_KEY_DOWN && optionCanMoveDown(option)) {
            assert(open_buffer);//NOTE: Must have an open buffer to do this option
            endGapBuffer(b);

            if(open_buffer->moveVertical_xPos < 0) {
                open_buffer->moveVertical_xPos = getXposAtInLine(b, &editorState->font, editorState->fontScale);
            }

            assert(open_buffer->moveVertical_xPos  >= 0);
            int new_cursor_pos_inBytes = getCusorPosLineBelow(b, &editorState->font, editorState->fontScale, open_buffer);

            updateNewCursorPos(b, new_cursor_pos_inBytes, selectable_state);				   	
        }      
    }
}