# Init2.c Implementation Report: Quest Text Storage System

## Overview
This report details how to implement quest text storage following the existing .txt/.raw file system architecture in Sil-qh, based on analysis of `init2.c` and related files.

## Current System Architecture

### File Processing Flow
```
lib/edit/character.txt  →  lib/data/character.raw
    ↓                           ↑
parse_c_info()         →    Binary Data
    ↓                           ↑  
init_c_info()          →    c_info[] array
```

### Key Components Analysis

#### 1. Data Structure Definition (Required in angband.h)
```c
typedef struct quest_type quest_type;

struct quest_type
{
    u32b name;              /* Name offset in quest names */
    u32b text;              /* Text offset in quest text storage */
    
    byte qtype;             /* Quest type: 0=vault-based, 1=roulet-based */
    byte associated_oath;   /* Oath unlocked: OATH_SILENCE, OATH_SMITH, etc. */
    byte reward_type;       /* Reward flags: STATS|ABILITIES|FLAGS|SKILLS|ARTIFACT */
    
    /* Challenge and reward descriptions */
    u32b challenge_text;    /* Challenge description offset */
    u32b reward_text;       /* Reward description offset */
    
    /* Stat bonuses */
    s16b stat_bonus[4];     /* STR, DEX, CON, GRA bonuses */
    
    /* Skill bonuses */
    s16b skill_bonus[8];    /* Bonus for each skill type */
    
    /* Special abilities/items */
    s16b abilities[4];      /* Ability IDs granted */
    byte ability_count;     /* Number of abilities */
    
    /* Quest text arrays */
    u32b init_text[10];     /* Initialization/quest giving text lines */
    u32b completion_text[10]; /* Completion/reward text lines */
    byte init_text_count;   /* Number of init text lines */
    byte completion_text_count; /* Number of completion text lines */
};
```

#### 2. Global Variables (Required in externs.h)
```c
/* Quest info */
extern quest_type *q_info;
extern char *q_name;
extern char *q_text;
extern header q_head;
```

#### 3. Initialization Function (Add to init2.c)
```c
/*
 * Initialize the "q_info" array
 */
static errr init_q_info(void)
{
    errr err;

    /* Init the header */
    init_header(&q_head, z_info->q_max, sizeof(quest_type));

#ifdef ALLOW_TEMPLATES

    /* Save a pointer to the parsing function */
    q_head.parse_info_txt = parse_q_info;

#endif /* ALLOW_TEMPLATES */

    err = init_info("quest", &q_head);

    /* Set the global variables */
    q_info = q_head.info_ptr;
    q_name = q_head.name_ptr;
    q_text = q_head.text_ptr;

    return (err);
}
```

#### 4. Parser Function (Add to init1.c)
```c
/*
 * Initialize the "q_info" array, by parsing an ascii "template" file
 */
errr parse_q_info(char* buf, header* head)
{
    int i;
    char *s, *t;
    
    /* Current entry */
    static quest_type* q_ptr = NULL;
    static int current_init_text = 0;
    static int current_completion_text = 0;

    /* Process 'Q' for "Quest/ID/Name" */
    if (buf[0] == 'Q')
    {
        /* Find the colon before the name */
        s = strchr(buf + 2, ':');
        if (!s) return (PARSE_ERROR_GENERIC);
        
        /* Nuke the colon, advance to the name */
        *s++ = '\0';
        if (!*s) return (PARSE_ERROR_GENERIC);

        /* Get the index */
        i = atoi(buf + 2);
        if (i <= error_idx) return (PARSE_ERROR_NON_SEQUENTIAL_RECORDS);
        if (i >= head->info_num) return (PARSE_ERROR_TOO_MANY_ENTRIES);
        error_idx = i;

        /* Point at the "info" */
        q_ptr = (quest_type*)head->info_ptr + i;
        
        /* Reset counters */
        current_init_text = 0;
        current_completion_text = 0;
        q_ptr->init_text_count = 0;
        q_ptr->completion_text_count = 0;
        q_ptr->ability_count = 0;

        /* Store the name */
        if (!(q_ptr->name = add_name(head, s)))
            return (PARSE_ERROR_OUT_OF_MEMORY);
    }

    /* Process 'T' for "Title" */
    else if (buf[0] == 'T')
    {
        if (!q_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);
        
        /* Store title text */
        if (!add_text(&q_ptr->text, head, buf + 2))
            return (PARSE_ERROR_OUT_OF_MEMORY);
    }

    /* Process 'C' for "Challenge" */
    else if (buf[0] == 'C')
    {
        if (!q_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);
        
        /* Store challenge description */
        if (!add_text(&q_ptr->challenge_text, head, buf + 2))
            return (PARSE_ERROR_OUT_OF_MEMORY);
    }

    /* Process 'O' for "Oath" */
    else if (buf[0] == 'O')
    {
        if (!q_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);
        
        /* Parse oath type */
        cptr oath_name = buf + 2;
        if (streq(oath_name, "OATH_SILENCE")) q_ptr->associated_oath = OATH_SILENCE;
        else if (streq(oath_name, "OATH_SMITH")) q_ptr->associated_oath = OATH_SMITH;
        else if (streq(oath_name, "OATH_IRON")) q_ptr->associated_oath = OATH_IRON;
        else if (streq(oath_name, "OATH_MERCY")) q_ptr->associated_oath = OATH_MERCY;
        else if (streq(oath_name, "NONE")) q_ptr->associated_oath = 0;
        else return (PARSE_ERROR_GENERIC);
    }

    /* Process 'Y' for "tYpe" (quest type) */
    else if (buf[0] == 'Y')
    {
        if (!q_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);
        
        q_ptr->qtype = (byte)atoi(buf + 2);
        if (q_ptr->qtype > 1) return (PARSE_ERROR_GENERIC);
    }

    /* Process 'S' for "Stats" */
    else if (buf[0] == 'S')
    {
        if (!q_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);
        
        /* Parse stat bonuses: S:str:dex:con:gra */
        if (4 != sscanf(buf + 2, "%d:%d:%d:%d", 
                        &q_ptr->stat_bonus[0], &q_ptr->stat_bonus[1],
                        &q_ptr->stat_bonus[2], &q_ptr->stat_bonus[3]))
            return (PARSE_ERROR_GENERIC);
    }

    /* Process 'K' for "sKills" */
    else if (buf[0] == 'K')
    {
        if (!q_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);
        
        /* Parse skill:bonus pairs */
        s = buf + 2;
        while (*s)
        {
            /* Find colon */
            t = strchr(s, ':');
            if (!t) break;
            *t++ = '\0';
            
            /* Parse skill type */
            int skill_type = -1;
            if (streq(s, "SMT")) skill_type = S_SMT;
            else if (streq(s, "MEL")) skill_type = S_MEL;
            /* ... add other skills ... */
            
            if (skill_type >= 0 && skill_type < 8)
            {
                q_ptr->skill_bonus[skill_type] = (s16b)atoi(t);
            }
            
            /* Find next entry */
            s = strchr(t, ' ');
            if (s) s++;
            else break;
        }
    }

    /* Process 'A' for "Abilities" */
    else if (buf[0] == 'A')
    {
        if (!q_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);
        
        /* Store ability reference */
        if (q_ptr->ability_count < 4)
        {
            /* Could store ability ID or text reference */
            if (!add_text(&q_ptr->abilities[q_ptr->ability_count], head, buf + 2))
                return (PARSE_ERROR_OUT_OF_MEMORY);
            q_ptr->ability_count++;
        }
    }

    /* Process 'I' for "Initialization text" */
    else if (buf[0] == 'I')
    {
        if (!q_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);
        
        if (current_init_text < 10)
        {
            if (!add_text(&q_ptr->init_text[current_init_text], head, buf + 2))
                return (PARSE_ERROR_OUT_OF_MEMORY);
            current_init_text++;
            q_ptr->init_text_count = current_init_text;
        }
    }

    /* Process 'W' for "Win/completion text" */
    else if (buf[0] == 'W')
    {
        if (!q_ptr) return (PARSE_ERROR_MISSING_RECORD_HEADER);
        
        if (current_completion_text < 10)
        {
            if (!add_text(&q_ptr->completion_text[current_completion_text], head, buf + 2))
                return (PARSE_ERROR_OUT_OF_MEMORY);
            current_completion_text++;
            q_ptr->completion_text_count = current_completion_text;
        }
    }

    else
    {
        /* Oops */
        return (PARSE_ERROR_UNDEFINED_DIRECTIVE);
    }

    /* Success */
    return (0);
}
```

#### 5. Integration into Main Initialization (Modify init_angband() in init2.c)
```c
    /* Initialize quest info */
    if (init_q_info()) quit("Cannot initialize quests");
```

Add this call after the existing init calls (around line 1900).

#### 6. Memory Management (Add to variable.c)
```c
/* Quest variables */
quest_type *q_info;
char *q_name;
char *q_text;
header q_head;
```

#### 7. Z-info Integration (Add to limits.txt)
```
# Quest info
M:Q:4
```

This tells the system to allocate space for 4 quests.

## Implementation Steps

### Phase 1: Core Structure Setup
1. **Add quest_type definition to angband.h**
   - Add after existing type definitions (around line 1500)
   - Include all required fields for quest data

2. **Add global variables to externs.h**
   - Add quest-related extern declarations
   - Follow pattern of existing info arrays

3. **Update limits.txt**
   - Add M:Q:4 entry to define maximum quest count
   - Increment version if needed

### Phase 2: Parser Implementation
1. **Add parse_q_info() to init1.c**
   - Implement all field parsers (Q:, T:, C:, O:, Y:, S:, K:, A:, I:, W:)
   - Handle text storage using add_text() pattern
   - Include error checking and validation

2. **Add init_q_info() to init2.c**
   - Follow pattern of existing init functions
   - Set up header and parsing function pointer
   - Handle memory allocation

3. **Integrate into main initialization**
   - Add init_q_info() call to init_angband()
   - Ensure proper error handling

### Phase 3: File Creation
1. **Create lib/edit/quest.txt**
   - Follow format defined in quest_text_storage_plan.md
   - Include all four quest definitions
   - Test parsing with version stamp

2. **Test compilation**
   - Verify quest.raw file generation
   - Check memory allocation and data loading
   - Validate quest data accessibility

### Phase 4: Integration with Existing Code
1. **Update quest functions in xtra2.c**
   - Replace hardcoded text with q_text lookups
   - Use q_info array for quest data
   - Implement dynamic text substitution

2. **Modify quest status display**
   - Use q_info data for status display
   - Access challenge and reward descriptions
   - Handle completion state properly

## File Dependencies

### Required Changes
- **angband.h**: Add quest_type definition
- **externs.h**: Add quest global variables  
- **init1.c**: Add parse_q_info() function
- **init2.c**: Add init_q_info() function and integration
- **variable.c**: Add quest variable definitions
- **lib/edit/limits.txt**: Add quest count limit
- **lib/edit/quest.txt**: Create quest data file

### Optional Enhancements
- **tables.c**: Add quest type name arrays for debugging
- **util.c**: Add quest utility functions
- **cmd4.c**: Add quest information display functions

## Error Handling Strategy

### Parse Errors
- Use existing PARSE_ERROR_* constants
- Provide meaningful error messages in display_parse_error()
- Validate all numeric ranges and field formats

### Runtime Errors
- Check q_info bounds before array access
- Validate quest IDs in function parameters
- Handle missing or corrupted quest data gracefully

### Memory Management
- Follow existing pattern of header-based allocation
- Use add_text() and add_name() for string storage
- Ensure proper cleanup in shutdown sequences

## Performance Considerations

### Memory Usage
- Quest data loaded once at startup (similar to character.txt)
- Text stored efficiently in shared string pool
- Minimal runtime memory allocation

### Access Patterns
- Direct array access by quest ID: q_info[quest_id]
- Text lookup via offset: q_text + q_info[id].text_offset
- No dynamic allocation during gameplay

### Compatibility
- Backward compatible with existing save files
- No changes to save/load formats required
- Quest state remains in player structure

## Testing Strategy

### Unit Tests
1. **Parser validation**: Test all field types and edge cases
2. **Memory integrity**: Verify no leaks or corruption
3. **Data access**: Validate array bounds and text retrieval

### Integration Tests  
1. **Game startup**: Verify quest loading and initialization
2. **Quest interaction**: Test quest giver dialogues use correct text
3. **Status display**: Confirm quest status shows proper information

### Regression Tests
1. **Save compatibility**: Ensure old saves still load
2. **Character data**: Verify character.txt still works correctly
3. **Performance**: Check startup time impact is minimal

## Migration Path

### Development Sequence
1. Implement core structure without changing existing code
2. Add quest.txt file and verify parsing works
3. Gradually replace hardcoded text with data lookups
4. Add new features (quest types, enhanced display)
5. Remove deprecated hardcoded quest text

### Rollback Strategy
- Keep existing hardcoded quest text as fallback
- Use feature flags to enable/disable quest text system
- Maintain parallel code paths during transition

This implementation follows Sil-qh's established patterns and ensures consistency with the existing codebase while providing a robust foundation for the quest text storage system.
