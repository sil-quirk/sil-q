/**
 * \file main-cocoa.m
 * \brief OS X front end
 *
 * Copyright (c) 2011 Peter Ammon
 *
 * This work is free software; you can redistribute it and/or modify it
 * under the terms of either:
 *
 * a) the GNU General Public License as published by the Free Software
 *    Foundation, version 2, or
 *
 * b) the "Angband licence":
 *    This software may be copied and distributed for educational, research,
 *    and not for profit purposes provided that this copyright and statement
 *    are included in all such copies.  Other copyrights may also apply.
 */

#include "angband.h"
////#include "cmds.h"
////#include "dungeon.h"
////#include "files.h"
////#include "init.h"
////#include "grafmode.h"

#if defined(SAFE_DIRECTORY)
#import "buildid.h"
#endif

/* #define NSLog(...) ; */

#if defined(MACH_O_CARBON)

/** Default creator signature */
//// Sil-y: changed ANGBAND_CREATOR to SIL_CREATOR throughout
////        change with new version
#ifndef SIL_CREATOR
# define SIL_CREATOR 'Sil1'
#endif

/* Mac headers */
#import "cocoa/AppDelegate.h"
#include <Carbon/Carbon.h> /* For keycodes */

static NSString * const AngbandDirectoryNameLib = @"lib";
////static NSString * const AngbandDirectoryNameBase = @"Angband";

static NSString * const FallbackFontName = @"Monaco";
static float FallbackFontSizeMain = 12.0f;
static float FallbackFontSizeSub = 9.0f;
static float FallbackFontSizeMonster = 11.0f;
static NSString * const AngbandTerminalsDefaultsKey = @"Terminals";
static NSString * const AngbandTerminalRowsDefaultsKey = @"Rows";
static NSString * const AngbandTerminalColumnsDefaultsKey = @"Columns";
static NSString * const AngbandTerminalVisibleDefaultsKey = @"Visible";
static NSString * const AngbandGraphicsDefaultsKey = @"GraphicsID";
static NSString * const AngbandFrameRateDefaultsKey = @"FramesPerSecond";
static NSString * const AngbandSoundDefaultsKey = @"AllowSound";
static NSInteger const AngbandWindowMenuItemTagBase = 1000;
static NSInteger const AngbandCommandMenuItemTagBase = 2000;

/**
 * We can blit to a large layer or image and then scale it down during live
 * resize, which makes resizing much faster, at the cost of some image quality
 * during resizing
 */
#ifndef USE_LIVE_RESIZE_CACHE
////# define USE_LIVE_RESIZE_CACHE 1
# define USE_LIVE_RESIZE_CACHE 0
#endif

/** Application defined event numbers */
enum
{
    AngbandEventWakeup = 1
};

/** Redeclare some 10.7 constants and methods so we can build on 10.6 */
enum
{
    Angband_NSWindowCollectionBehaviorFullScreenPrimary = 1 << 7,
    Angband_NSWindowCollectionBehaviorFullScreenAuxiliary = 1 << 8
};

@interface NSWindow (AngbandLionRedeclares)
- (void)setRestorable:(BOOL)flag;
@end

/** Delay handling of pre-emptive "quit" event */
static BOOL quit_when_ready = NO;

/** Set to indicate the game is over and we can quit without delay */
static BOOL game_is_finished = NO;

/** Our frames per second (e.g. 60). A value of 0 means unthrottled. */
static int frames_per_second;

@class AngbandView;

#if 0
/**
 * Load sound effects based on sound.cfg within the xtra/sound directory;
 * bridge to Cocoa to use NSSound for simple loading and playback, avoiding
 * I/O latency by caching all sounds at the start.  Inherits full sound
 * format support from Quicktime base/plugins.
 * pelpel favoured a plist-based parser for the future but .cfg support
 * improves cross-platform compatibility.
 */
@interface AngbandSoundCatalog : NSObject {
@private
    /**
     * Stores instances of NSSound keyed by path so the same sound can be
     * used for multiple events.
     */
    NSMutableDictionary *soundsByPath;
    /**
     * Stores arrays of NSSound keyed by event number.
     */
    NSMutableDictionary *soundArraysByEvent;
}

/**
 * If NO, then playSound effectively becomes a do nothing operation.
 */
@property (getter=isEnabled) BOOL enabled;

/**
 * Set up for lazy initialization in playSound().  Set enabled to NO.
 */
- (id)init;

/**
 * If self.enabled is YES and the given event has one or more sounds
 * corresponding to it in the catalog, plays one of those sounds, chosen at
 * random.
 */
- (void)playSound:(int)event;

/**
 * Impose an arbitrary limit on the number of possible samples per event.
 * Currently not declaring this as a class property for compatibility with
 * versions of Xcode prior to 8.
 */
+ (int)maxSamples;

/**
 * Return the shared sound catalog instance, creating it if it does not
 * exist yet.  Currently not declaring this as a class property for
 * compatibility with versions of Xcode prior to 8.
 */
+ (AngbandSoundCatalog*)sharedSounds;

/**
 * Release any resources associated with shared sounds.
 */
+ (void)clearSharedSounds;

@end

@implementation AngbandSoundCatalog

- (id)init
{
    if (self = [super init]) {
        self->soundsByPath = nil;
        self->soundArraysByEvent = nil;
        self->_enabled = NO;
    }
    return self;
}

- (void)playSound:(int)event
{
    if (! self.enabled) {
        return;
    }

    /* Initialize when the first sound is played. */
    if (self->soundArraysByEvent == nil) {
        /* Build the "sound" path */
        char path[2048];
        path_build(path, sizeof(path), ANGBAND_DIR_XTRA, "sound");
        ANGBAND_DIR_XTRA_SOUND = string_make(path);

        /* Find and open the config file */
        path_build(path, sizeof(path), ANGBAND_DIR_XTRA_SOUND, "sound.cfg");
        ang_file *fff = file_open(path, MODE_READ, -1);

        /* Handle errors */
        if (!fff) {
            NSLog(@"The sound configuration file could not be opened.");
            return;
        }

        self->soundsByPath = [[NSMutableDictionary alloc] init];
        self->soundArraysByEvent = [[NSMutableDictionary alloc] init];
        @autoreleasepool {
            /*
             * This loop may take a while depending on the count and size of
             * samples to load.
             */

            /* Parse the file */
            /* Lines are always of the form "name = sample [sample ...]" */
            char buffer[2048];
            while (file_getl(fff, buffer, sizeof(buffer))) {
                char *msg_name;
                char *cfg_sample_list;
                char *search;
                char *cur_token;
                char *next_token;
                int lookup_result;

                /* Skip anything not beginning with an alphabetic character */
                if (!buffer[0] || !isalpha((unsigned char)buffer[0])) continue;

                /* Split the line into two; message name, and the rest */
                search = strchr(buffer, ' ');
                cfg_sample_list = strchr(search + 1, ' ');
                if (!search) continue;
                if (!cfg_sample_list) continue;

                /* Set the message name, and terminate at first space */
                msg_name = buffer;
                search[0] = '\0';

                /* Make sure this is a valid event name */
                lookup_result = message_lookup_by_sound_name(msg_name);
                if (lookup_result < 0) continue;

                /*
                 * Advance the sample list pointer so it's at the beginning of
                 * text.
                 */
                cfg_sample_list++;
                if (!cfg_sample_list[0]) continue;

                /* Terminate the current token */
                cur_token = cfg_sample_list;
                search = strchr(cur_token, ' ');
                if (search) {
                    search[0] = '\0';
                    next_token = search + 1;
                } else {
                    next_token = NULL;
                }

                /*
                 * Now we find all the sample names and add them one by one
                 */
                while (cur_token) {
                    NSMutableArray *soundSamples =
                        [self->soundArraysByEvent
                             objectForKey:[NSNumber numberWithInteger:lookup_result]];
                    if (soundSamples == nil) {
                        soundSamples = [[NSMutableArray alloc] init];
                        [self->soundArraysByEvent
                            setObject:soundSamples
                            forKey:[NSNumber numberWithInteger:lookup_result]];
                    }
                    int num = (int) soundSamples.count;

                    /* Don't allow too many samples */
                    if (num >= [AngbandSoundCatalog maxSamples]) break;

                    NSString *token_string =
                        [NSString stringWithUTF8String:cur_token];
                    NSSound *sound =
                        [self->soundsByPath objectForKey:token_string];

                    if (! sound) {
                        /*
                         * We have to load the sound. Build the path to the
                         * sample.
                         */
                        path_build(path, sizeof(path), ANGBAND_DIR_XTRA_SOUND,
                                   cur_token);
                        if (file_exists(path)) {
                            /* Load the sound into memory */
                            sound = [[NSSound alloc]
                                         initWithContentsOfFile:[NSString stringWithUTF8String:path]
                                         byReference:YES];
                            if (sound) {
                                [self->soundsByPath setObject:sound
                                            forKey:token_string];
                            }
                        }
                    }

                    /* Store it if we loaded it */
                    if (sound) {
                        [soundSamples addObject:sound];
                    }

                    /* Figure out next token */
                    cur_token = next_token;
                    if (next_token) {
                        /* Try to find a space */
                        search = strchr(cur_token ' ');

                        /*
                         * If we can find one, terminate, and set new "next".
                         */
                        if (search) {
                            search[0] = '\0';
                            next_token = search + 1;
                        } else {
                            /* Otherwise prevent infinite looping */
                            next_token = NULL;
                        }
                    }
                }
            }
        }

        /* Close the file */
        file_close(fff);
    }

    @autoreleasepool {
        NSMutableArray *samples =
            [self->soundArraysByEvent
                 objectForKey:[NSNumber numberWithInteger:event]];

        if (samples == nil || samples.count == 0) {
            return;
        }

        /* Choose a random event. */
        int s = rand_int((int) samples.count);
        NSSound *sound = samples[s];

        if ([sound isPlaying])
            [sound stop];

        /* Play the sound. */
        [sound play];
    }
}

+ (int)maxSamples
{
    return 16;
}

/**
 * For sharedSounds and clearSharedSounds.
 */
static __strong AngbandSoundCatalog *gSharedSounds = nil;

+ (AngbandSoundCatalog*)sharedSounds
{
    if (gSharedSounds == nil) {
        gSharedSounds = [[AngbandSoundCatalog alloc] init];
    }
    return gSharedSounds;
}

+ (void)clearSharedSounds
{
    gSharedSounds = nil;
}

@end
#endif

/**
 * To handle fonts where an individual glyph's bounding box can extend into
 * neighboring columns, Term_curs_cocoa(), Term_pict_cocoa(),
 * Term_text_cocoa(), and Term_wipe_cocoa() merely record what needs to be
 * done with the actual drawing happening in response to the notification to
 * flush all rows, the TERM_XTRA_FRESH case in Term_xtra_cocoa().
 */
enum PendingCellChangeType {
    CELL_CHANGE_NONE = 0,
    CELL_CHANGE_WIPE,
    CELL_CHANGE_TEXT,
    CELL_CHANGE_TILE
};
#define PICT_MASK_NONE (0x0)
#define PICT_MASK_ALERT (0x1)
#define PICT_MASK_GLOW (0x2)
struct PendingTextChange {
    wchar_t glyph;
    int color;
};
struct PendingTileChange {
    char fgdCol; /**< column coordinate, within the tile set, for the
                     foreground tile */
    char fgdRow; /**< row coordinate, within the tile set, for the
                     foreground tile */
    char bckCol; /**< column coordinate, within the tile set, for the
                     background tile */
    char bckRow; /**< row coordinate within the tile set, for the
                     background tile */
    int mask;    /**< PICT_MASK_NONE or a bitwise-or of one or more of
                     PICT_MAX_ALERT and PICT_MASK_GLOW */
};
struct PendingCellChange {
    union { struct PendingTextChange txc; struct PendingTileChange tic; } v;
    enum PendingCellChangeType changeType;
};

@interface PendingTermChanges : NSObject {
@private
    int *colBounds;
    struct PendingCellChange **changesByRow;
}

/**
 * Returns YES if nCol and nRow are a feasible size for the pending changes.
 * Otherwise, returns NO.
 */
+ (BOOL)isValidSize:(int)nCol rows:(int)nRow;

/**
 * Initialize with zero columns and zero rows.
 */
- (id)init;

/**
 * Initialize with nCol columns and nRow rows.  No changes will be marked.
 */
- (id)initWithColumnsRows:(int)nCol rows:(int)nRow NS_DESIGNATED_INITIALIZER;

/**
 * Clears all marked changes.
 */
- (void)clear;

/**
 * Changes the bounds over which changes are recorded.  Has the side effect
 * of clearing any marked changes.  Will throw an exception if nCol or nRow
 * is negative.
 */
- (void)resize:(int)nCol rows:(int)nRow;

/**
 * Mark the cell, (iCol, iRow), as having changed text.
 */
- (void)markTextChange:(int)iCol row:(int)iRow glyph:(wchar_t)g color:(int)c;

/**
 * Mark the cell, (iCol, iRow), as having a changed tile.
 */
- (void)markTileChange:(int)iCol row:(int)iRow
    foregroundCol:(char)fc foregroundRow:(char)fr
    backgroundCol:(char)bc backgroundRow:(char)br
    mask:(int)m;

/**
 * Mark the cells from (iCol, iRow) to (iCol + nCol - 1, iRow) as wiped.
 */
- (void)markWipeRange:(int)iCol row:(int)iRow n:(int)nCol;

/**
 * Mark the location of the cursor.  The cursor will be the standard size:
 * one cell.
 */
- (void)markCursor:(int)iCol row:(int)iRow;

/**
 * Mark the location of the cursor.  The cursor will be w cells wide and
 * h cells tall, and the given location is the position of the upper left
 * corner.
 */
- (void)markBigCursor:(int)iCol row:(int)iRow
            cellsWide:(int)w cellsHigh:(int)h;

/**
 * Return the zero-based index of the first column changed for the given
 * zero-based row index.  If there are no changes in the row, the returned
 * value will be the number of columns.
 */
- (int)getFirstChangedColumnInRow:(int)iRow;

/**
 * Return the zero-based index of the last column changed for the given
 * zero-based row index.  If there are no changes in the row, the returned
 * value will be -1.
 */
- (int)getLastChangedColumnInRow:(int)iRow;

/**
 * Return the type of change at the given cell, (iCol, iRow).
 */
- (enum PendingCellChangeType)getCellChangeType:(int)iCol row:(int)iRow;

/**
 * Return the nature of a text change at the given cell, (iCol, iRow).
 * Will throw an exception if [obj getCellChangeType:iCol row:iRow] is
 * neither CELL_CHANGE_TEXT nor CELL_CHANGE_WIPE.
 */
- (struct PendingTextChange)getCellTextChange:(int)iCol row:(int)iRow;

/**
 * Return the nature of a tile change at the given cell, (iCol, iRow).
 * Will throw an exception if [obj getCellChangeType:iCol row:iRow] is
 * different than CELL_CHANGE_TILE.
 */
- (struct PendingTileChange)getCellTileChange:(int)iCol row:(int)iRow;

/**
 * Is the number of columns for recording changes.
 */
@property (readonly) int columnCount;

/**
 * Is the number of rows for recording changes.
 */
@property (readonly) int rowCount;

/**
 * Will be YES if there are any pending changes to locations rendered as text.
 * Otherwise, it will be NO.
 */
@property (readonly) BOOL hasTextChanges;

/**
 * Will be YES if there are any pending changes to locations rendered as tiles.
 * Otherwise, it will be NO.
 */
@property (readonly) BOOL hasTileChanges;

/**
 * Will be YES if there are any pending wipes.  Otherwise, it will be NO.
 */
@property (readonly) BOOL hasWipeChanges;

/**
 * Is the zero-based index of the first row with changes.  Will be equal to
 * the number of rows if there are no changes.
 */
@property (readonly) int firstChangedRow;

/**
 * Is the zero-based index of the last row with changes.  Will be equal to
 * -1 if there are no changes.
 */
@property (readonly) int lastChangedRow;

/**
 * Is the zero-based index for the column with the upper left corner of the
 * cursor.  It will be -1 if the cursor position has not been set since the
 * changes were cleared.
 */
@property (readonly) int cursorColumn;

/**
 * Is the zero-based index for the row with the upper left corner of the
 * cursor.  It will be -1 if the cursor position has not been set since the
 * changes were cleared.
 */
@property (readonly) int cursorRow;

/**
 * Is the cursor width in number of cells.
 */
@property (readonly) int cursorWidth;

/**
 * Is the cursor height in number of cells.
 */
@property (readonly) int cursorHeight;

/**
 * This is a helper for the mark* messages.
 */
- (void)setupForChange:(int)iCol row:(int)iRow n:(int)nCol;

/**
 * Throw an exception if the given range of column indices is invalid
 * (including non-positive values for nCol).
 */
- (void)checkColumnIndices:(int)iCol n:(int)nCol;

/**
 * Throw an exception if the given row index is invalid.
 */
- (void)checkRowIndex:(int)iRow;

@end

@implementation PendingTermChanges

+ (BOOL) isValidSize:(int)nCol rows:(int)nRow
{
    if (nCol < 0
            || (size_t) nCol > SIZE_MAX / sizeof(struct PendingCellChange)
            || nRow < 0
            || (size_t) nRow > SIZE_MAX / sizeof(struct PendingCellChange*)
            || (size_t) nRow > SIZE_MAX / (2 * sizeof(int))) {
        return NO;
    }
    return YES;
}

- (id)init
{
    return [self initWithColumnsRows:0 rows:0];
}

- (id)initWithColumnsRows:(int)nCol rows:(int)nRow
{
    if (self = [super init]) {
        if (! [PendingTermChanges isValidSize:nCol rows:nRow]) {
            return nil;
        }
        self->colBounds = malloc((size_t) 2 * sizeof(int) * nRow);
        if (self->colBounds == 0 && nRow > 0) {
            return nil;
        }
        self->changesByRow = calloc(nRow, sizeof(struct PendingCellChange*));
        if (self->changesByRow == 0 && nRow > 0) {
            free(self->colBounds);
            return nil;
        }
        for (int i = 0; i < nRow + nRow; i += 2) {
            self->colBounds[i] = nCol;
            self->colBounds[i + 1] = -1;
        }
        self->_columnCount = nCol;
        self->_rowCount = nRow;
        self->_hasTextChanges = NO;
        self->_hasTileChanges = NO;
        self->_hasWipeChanges = NO;
        self->_firstChangedRow = nRow;
        self->_lastChangedRow = -1;
        self->_cursorColumn = -1;
        self->_cursorRow = -1;
        self->_cursorWidth = 1;
        self->_cursorHeight = 1;
    }
    return self;
}

- (void)dealloc
{
    if (self->changesByRow != 0) {
        for (int i = 0; i < self.rowCount; ++i) {
            if (self->changesByRow[i] != 0) {
                free(self->changesByRow[i]);
                self->changesByRow[i] = 0;
            }
        }
        free(self->changesByRow);
        self->changesByRow = 0;
    }
    if (self->colBounds != 0) {
        free(self->colBounds);
        self->colBounds = 0;
    }
}

- (void)clear
{
    for (int i = 0; i < self.rowCount; ++i) {
        self->colBounds[i + i] = self.columnCount;
        self->colBounds[i + i + 1] = -1;
        if (self->changesByRow[i] != 0) {
            free(self->changesByRow[i]);
            self->changesByRow[i] = 0;
        }
    }
    self->_hasTextChanges = NO;
    self->_hasTileChanges = NO;
    self->_hasWipeChanges = NO;
    self->_firstChangedRow = self.rowCount;
    self->_lastChangedRow = -1;
    self->_cursorColumn = -1;
    self->_cursorRow = -1;
    self->_cursorWidth = 1;
    self->_cursorHeight = 1;
}

- (void)resize:(int)nCol rows:(int)nRow
{
    if (! [PendingTermChanges isValidSize:nCol rows:nRow]) {
        NSException *exc = [NSException
                               exceptionWithName:@"PendingTermChangesRowsColumns"
                               reason:@"resize called with number of columns or rows that is negative or too large"
                               userInfo:nil];
        @throw exc;
    }
    int *cb = malloc((size_t) 2 * sizeof(int) * nRow);
    struct PendingCellChange** cbr =
        calloc(nRow, sizeof(struct PendingCellChange*));
    int i;

    if ((cb == 0 || cbr == 0) && nRow > 0) {
        if (cbr != 0) {
            free(cbr);
        }
        if (cb != 0) {
            free(cb);
        }
        NSException *exc = [NSException
                               exceptionWithName:@"OutOfMemory"
                               reason:@"resize called for PendingTermChanges"
                               userInfo:nil];
        @throw exc;
    }

    for (i = 0; i < nRow; ++i) {
        cb[i + i] = nCol;
        cb[i + i + 1] = -1;
    }
    if (self->changesByRow != 0) {
        for (i = 0; i < self.rowCount; ++i) {
            if (self->changesByRow[i] != 0) {
                free(self->changesByRow[i]);
                self->changesByRow[i] = 0;
            }
        }
        free(self->changesByRow);
    }
    if (self->colBounds != 0) {
        free(self->colBounds);
    }

    self->colBounds = cb;
    self->changesByRow = cbr;
    self->_columnCount = nCol;
    self->_rowCount = nRow;
    self->_hasTextChanges = NO;
    self->_hasTileChanges = NO;
    self->_hasWipeChanges = NO;
    self->_firstChangedRow = self.rowCount;
    self->_lastChangedRow = -1;
    self->_cursorColumn = -1;
    self->_cursorRow = -1;
    self->_cursorWidth = 1;
    self->_cursorHeight = 1;
}

- (void)markTextChange:(int)iCol row:(int)iRow glyph:(wchar_t)g color:(int)c
{
    [self setupForChange:iCol row:iRow n:1];
    struct PendingCellChange *pcc = self->changesByRow[iRow] + iCol;
    pcc->v.txc.glyph = g;
    pcc->v.txc.color = c;
    pcc->changeType = CELL_CHANGE_TEXT;
    self->_hasTextChanges = YES;
}

- (void)markTileChange:(int)iCol row:(int)iRow
         foregroundCol:(char)fc foregroundRow:(char)fr
         backgroundCol:(char)bc backgroundRow:(char)br
         mask:(int)m
{
    [self setupForChange:iCol row:iRow n:1];
    struct PendingCellChange *pcc = self->changesByRow[iRow] + iCol;
    pcc->v.tic.fgdCol = fc;
    pcc->v.tic.fgdRow = fr;
    pcc->v.tic.bckCol = bc;
    pcc->v.tic.bckRow = br;
    pcc->v.tic.mask = m;
    pcc->changeType = CELL_CHANGE_TILE;
    self->_hasTileChanges = YES;
}

- (void)markWipeRange:(int)iCol row:(int)iRow n:(int)nCol
{
    [self setupForChange:iCol row:iRow n:nCol];
    struct PendingCellChange *pcc = self->changesByRow[iRow] + iCol;
    for (int i = 0; i < nCol; ++i) {
        pcc[i].v.txc.glyph = 0;
        pcc[i].v.txc.color = 0;
        pcc[i].changeType = CELL_CHANGE_WIPE;
    }
    self->_hasWipeChanges = YES;
}

- (void)markCursor:(int)iCol row:(int)iRow
{
    /* Allow negative indices to indicate an invalid cursor. */
    [self checkColumnIndices:((iCol >= 0) ? iCol : 0) n:1];
    [self checkRowIndex:((iRow >= 0) ? iRow : 0)];
    self->_cursorColumn = iCol;
    self->_cursorRow = iRow;
    self->_cursorWidth = 1;
    self->_cursorHeight = 1;
}

- (void)markBigCursor:(int)iCol row:(int)iRow
            cellsWide:(int)w cellsHigh:(int)h
{
    /* Allow negative indices to indicate an invalid cursor. */
    [self checkColumnIndices:((iCol >= 0) ? iCol : 0) n:1];
    [self checkRowIndex:((iRow >= 0) ? iRow : 0)];
    if (w < 1 || h < 1) {
        NSException *exc = [NSException
                               exceptionWithName:@"InvalidCursorDimensions"
                               reason:@"markBigCursor called for PendingTermChanges"
                               userInfo:nil];
        @throw exc;
    }
    self->_cursorColumn = iCol;
    self->_cursorRow = iRow;
    self->_cursorWidth = w;
    self->_cursorHeight = h;
}

- (void)setupForChange:(int)iCol row:(int)iRow n:(int)nCol
{
    [self checkColumnIndices:iCol n:nCol];
    [self checkRowIndex:iRow];
    if (self->changesByRow[iRow] == 0) {
        self->changesByRow[iRow] =
            malloc(self.columnCount * sizeof(struct PendingCellChange));
        if (self->changesByRow[iRow] == 0 && self.columnCount > 0) {
            NSException *exc = [NSException
                                   exceptionWithName:@"OutOfMemory"
                                   reason:@"setupForChange called for PendingTermChanges"
                                   userInfo:nil];
            @throw exc;
        }
        struct PendingCellChange* pcc = self->changesByRow[iRow];
        for (int i = 0; i < self.columnCount; ++i) {
            pcc[i].changeType = CELL_CHANGE_NONE;
        }
    }
    if (self.firstChangedRow > iRow) {
        self->_firstChangedRow = iRow;
    }
    if (self.lastChangedRow < iRow) {
        self->_lastChangedRow = iRow;
    }
    if ([self getFirstChangedColumnInRow:iRow] > iCol) {
        self->colBounds[iRow + iRow] = iCol;
    }
    if ([self getLastChangedColumnInRow:iRow] < iCol + nCol - 1) {
        self->colBounds[iRow + iRow + 1] = iCol + nCol - 1;
    }
}

- (int)getFirstChangedColumnInRow:(int)iRow
{
    [self checkRowIndex:iRow];
    return self->colBounds[iRow + iRow];
}

- (int)getLastChangedColumnInRow:(int)iRow
{
    [self checkRowIndex:iRow];
    return self->colBounds[iRow + iRow + 1];
}

- (enum PendingCellChangeType)getCellChangeType:(int)iCol row:(int)iRow
{
    [self checkColumnIndices:iCol n:1];
    [self checkRowIndex:iRow];
    if (iRow < self.firstChangedRow || iRow > self.lastChangedRow) {
        return CELL_CHANGE_NONE;
    }
    if (iCol < [self getFirstChangedColumnInRow:iRow]
            || iCol > [self getLastChangedColumnInRow:iRow]) {
        return CELL_CHANGE_NONE;
    }
    return self->changesByRow[iRow][iCol].changeType;
}

- (struct PendingTextChange)getCellTextChange:(int)iCol row:(int)iRow
{
    [self checkColumnIndices:iCol n:1];
    [self checkRowIndex:iRow];
    if (iRow < self.firstChangedRow || iRow > self.lastChangedRow
            || iCol < [self getFirstChangedColumnInRow:iRow]
            || iCol > [self getLastChangedColumnInRow:iRow]
            || (self->changesByRow[iRow][iCol].changeType != CELL_CHANGE_TEXT
            && self->changesByRow[iRow][iCol].changeType != CELL_CHANGE_WIPE)) {
        NSException *exc = [NSException
                               exceptionWithName:@"NotTextChange"
                               reason:@"getCellTextChange called for PendingTermChanges"
                               userInfo:nil];
        @throw exc;
    }
    return self->changesByRow[iRow][iCol].v.txc;
}

- (struct PendingTileChange)getCellTileChange:(int)iCol row:(int)iRow
{
    [self checkColumnIndices:iCol n:1];
    [self checkRowIndex:iRow];
    if (iRow < self.firstChangedRow || iRow > self.lastChangedRow
            || iCol < [self getFirstChangedColumnInRow:iRow]
            || iCol > [self getLastChangedColumnInRow:iRow]
            || self->changesByRow[iRow][iCol].changeType != CELL_CHANGE_TILE) {
        NSException *exc = [NSException
                               exceptionWithName:@"NotTileChange"
                               reason:@"getCellTileChange called for PendingTermChanges"
                               userInfo:nil];
        @throw exc;
    }
    return self->changesByRow[iRow][iCol].v.tic;
}

- (void)checkColumnIndices:(int)iCol n:(int)nCol
{
    if (iCol < 0) {
        NSException *exc = [NSException
                               exceptionWithName:@"InvalidColumnIndex"
                               reason:@"negative column index"
                               userInfo:nil];
        @throw exc;
    }
    if (iCol >= self.columnCount || iCol + nCol > self.columnCount) {
        NSException *exc = [NSException
                               exceptionWithName:@"InvalidColumnIndex"
                               reason:@"column index exceeds number of columns"
                               userInfo:nil];
        @throw exc;
    }
    if (nCol <= 0) {
        NSException *exc = [NSException
                               exceptionWithName:@"InvalidColumnIndex"
                               reason:@"empty column range"
                               userInfo:nil];
        @throw exc;
    }
}

- (void)checkRowIndex:(int)iRow
{
    if (iRow < 0) {
        NSException *exc = [NSException
                               exceptionWithName:@"InvalidRowIndex"
                               reason:@"negative row index"
                               userInfo:nil];
        @throw exc;
    }
    if (iRow >= self.rowCount) {
        NSException *exc = [NSException
                               exceptionWithName:@"InvalidRowIndex"
                               reason:@"row index exceeds number of rows"
                               userInfo:nil];
        @throw exc;
    }
}

@end


/** The max number of glyphs we support.  Currently this only affects
 * updateGlyphInfo() for the calculation of the tile size, fontAscender,
 * fontDescender, nColPre, and nColPost.  The rendering in drawWChar() will
 * work for a glyph not in updateGlyphInfo()'s set, though there may be
 * clipping or clearing artifacts because it wasn't included in
 * updateGlyphInfo()'s calculations.
 */
#define GLYPH_COUNT 256

/**
 * An AngbandContext represents a logical Term (i.e. what Angband thinks is
 * a window). This typically maps to one NSView, but may map to more than one
 * NSView (e.g. the Test and real screen saver view).
 */
@interface AngbandContext : NSObject <NSWindowDelegate>
{
@public

    /** The Angband term */
    term *terminal;

@private
    /** Is the last time we drew, so we can throttle drawing. */
    CFAbsoluteTime lastRefreshTime;

    /**
     * Whether we are currently in live resize, which affects how big we
     * render our image.
     */
    int inLiveResize;

    /** Flags whether or not a fullscreen transition is in progress. */
    BOOL inFullscreenTransition;
}

/** Column count, by default 80 */
@property int cols;
/** Row count, by default 24 */
@property int rows;

/** The size of the border between the window edge and the contents */
@property (readonly) NSSize borderSize;

/** Our array of views */
@property NSMutableArray *angbandViews;

/** The buffered image */
@property CGLayerRef angbandLayer;

/** The font of this context */
@property NSFont *angbandViewFont;

/** The size of one tile */
@property (readonly) NSSize tileSize;

/** Font's ascender */
@property (readonly) CGFloat fontAscender;
/** Font's descender */
@property (readonly) CGFloat fontDescender;

/** The number of colums before a text change that may need to be redrawn. */
@property (readonly) int nColPre;
/** The number of colums after a text change that may need to be redrawn. */
@property (readonly) int nColPost;

/** If this context owns a window, here it is. */
@property NSWindow *primaryWindow;

/** Is the record of changes to the contents for the next update. */
@property PendingTermChanges *changes;

@property (nonatomic, assign) BOOL hasSubwindowFlags;
@property (nonatomic, assign) BOOL windowVisibilityChecked;

- (void)drawRect:(NSRect)rect inView:(NSView *)view;

/** Called at initialization to set the term */
- (void)setTerm:(term *)t;

/** Called when the context is going down. */
- (void)dispose;

/** Returns the size of the image. */
- (NSSize)imageSize;

/** Return the rect for a tile at given coordinates. */
- (NSRect)rectInImageForTileAtX:(int)x Y:(int)y;

/** Draw the given wide character into the given tile rect. */
- (void)drawWChar:(wchar_t)wchar inRect:(NSRect)tile context:(CGContextRef)ctx;

/** Locks focus on the Angband image, and scales the CTM appropriately. */
- (CGContextRef)lockFocus;

/**
 * Locks focus on the Angband image but does NOT scale the CTM. Appropriate
 * for drawing hairlines.
 */
- (CGContextRef)lockFocusUnscaled;

/** Unlocks focus. */
- (void)unlockFocus;

/**
 * Returns the primary window for this angband context, creating it if
 * necessary
 */
- (NSWindow *)makePrimaryWindow;

/** Called to add a new Angband view */
- (void)addAngbandView:(AngbandView *)view;

/** Make the context aware that one of its views changed size */
- (void)angbandViewDidScale:(AngbandView *)view;

/** Handle becoming the main window */
- (void)windowDidBecomeMain:(NSNotification *)notification;

/** Return whether the context's primary window is ordered in or not */
- (BOOL)isOrderedIn;

/** Return whether the context's primary window is key */
- (BOOL)isMainWindow;

/** Invalidate the whole image */
- (void)setNeedsDisplay:(BOOL)val;

/** Invalidate part of the image, with the rect expressed in base coordinates */
- (void)setNeedsDisplayInBaseRect:(NSRect)rect;

/** Display (flush) our Angband views */
- (void)displayIfNeeded;

/** Resize context to size of contentRect, and optionally save size to
 * defaults */
- (void)resizeTerminalWithContentRect: (NSRect)contentRect saveToDefaults: (BOOL)saveToDefaults;

/**
 * Change the size bounds and size increments for the window associated with
 * the context.  termIdx is the index for the terminal:  pass it so this
 * function can be used when self->terminal has not yet been set.
 */
- (void)constrainWindowSize:(int)termIdx;

/** Called from the view to indicate that it is starting or ending live resize */
- (void)viewWillStartLiveResize:(AngbandView *)view;
- (void)viewDidEndLiveResize:(AngbandView *)view;
- (void)saveWindowVisibleToDefaults: (BOOL)windowVisible;
- (BOOL)windowVisibleUsingDefaults;

/* Class methods */
/**
 * Gets the default font for all contexts.  Currently not declaring this as
 * a class property for compatibility with versions of Xcode prior to 8.
 */
+ (NSFont*)defaultFont;
/**
 * Sets the default font for all contexts.
 */
+ (void)setDefaultFont:(NSFont*)font;

/** Internal method */
- (AngbandView *)activeView;

@end

/**
 * Generate a mask for the subwindow flags. The mask is just a safety check to
 * make sure that our windows show and hide as expected.  This function allows
 * for future changes to the set of flags without needed to update it here
 * (unless the underlying types change).
 */
static u32b AngbandMaskForValidSubwindowFlags(void)
{
    int windowFlagBits = sizeof(*(op_ptr->window_flag)) * CHAR_BIT;
    int maxBits = MIN( PW_MAX_FLAGS, windowFlagBits );
    u32b mask = 0;

    for( int i = 0; i < maxBits; i++ )
    {
        if( window_flag_desc[i] != NULL )
        {
            mask |= (1 << i);
        }
    }

    return mask;
}

/**
 * Check for changes in the subwindow flags and update window visibility.
 * This seems to be called for every user event, so we don't
 * want to do any unnecessary hiding or showing of windows.
 */
static void AngbandUpdateWindowVisibility(void)
{
    /*
     * Because this function is called frequently, we'll make the mask static.
     * It doesn't change between calls, as the flags themselves are hardcoded
     */
    static u32b validWindowFlagsMask = 0;
    BOOL anyChanged = NO;

    if( validWindowFlagsMask == 0 )
    {
        validWindowFlagsMask = AngbandMaskForValidSubwindowFlags();
    }

    /*
     * Loop through all of the subwindows and see if there is a change in the
     * flags. If so, show or hide the corresponding window. We don't care about
     * the flags themselves; we just want to know if any are set.
     */
    for( int i = 1; i < ANGBAND_TERM_MAX; i++ )
    {
        AngbandContext *angbandContext =
            (__bridge AngbandContext*) (angband_term[i]->data);

        if( angbandContext == nil )
        {
            continue;
        }

        /*
         * This horrible mess of flags is so that we can try to maintain some
         * user visibility preference. This should allow the user a window and
         * have it stay closed between application launches. However, this
         * means that when a subwindow is turned on, it will no longer appear
         * automatically. Angband has no concept of user control over window
         * visibility, other than the subwindow flags.
         */
        if( !angbandContext.windowVisibilityChecked )
        {
            if( [angbandContext windowVisibleUsingDefaults] )
            {
                [angbandContext.primaryWindow orderFront: nil];
                angbandContext.windowVisibilityChecked = YES;
                anyChanged = YES;
            }
            else if ([angbandContext.primaryWindow isVisible])
            {
                [angbandContext.primaryWindow close];
                angbandContext.windowVisibilityChecked = NO;
                anyChanged = YES;
            }
        }
        else
        {
            BOOL termHasSubwindowFlags = ((op_ptr->window_flag[i] & validWindowFlagsMask) > 0);

            if( angbandContext.hasSubwindowFlags && !termHasSubwindowFlags )
            {
                [angbandContext.primaryWindow close];
                angbandContext.hasSubwindowFlags = NO;
                [angbandContext saveWindowVisibleToDefaults: NO];
                anyChanged = YES;
            }
            else if( !angbandContext.hasSubwindowFlags && termHasSubwindowFlags )
            {
                [angbandContext.primaryWindow orderFront: nil];
                angbandContext.hasSubwindowFlags = YES;
                [angbandContext saveWindowVisibleToDefaults: YES];
                anyChanged = YES;
            }
        }
    }

    /* Make the main window key so that user events go to the right spot */
    if (anyChanged) {
        AngbandContext *mainWindow =
            (__bridge AngbandContext*) (angband_term[0]->data);
        [mainWindow.primaryWindow makeKeyAndOrderFront: nil];
    }
}

/**
 * ------------------------------------------------------------------------
 * Graphics support
 * ------------------------------------------------------------------------
 */

/**
 * The tile image
 */
static CGImageRef pict_image;

/**
 * Numbers of rows and columns in a tileset,
 * calculated by the PICT/PNG loading code
 */
static int pict_cols = 0;
static int pict_rows = 0;

/**
 * Width and height of each tile in the current tile set.
 */
static int pict_cell_width = 0;
static int pict_cell_height = 0;

/**
 * Will be nonzero if use_bigtile has changed since the last redraw.
 */
static int tile_multipliers_changed = 0;

/**
 * Helper function to check the various ways that graphics can be enabled.
 */
static BOOL graphics_are_enabled(void)
{
     return use_graphics != GRAPHICS_NONE;
}

/**
 * Hack -- game in progress
 */
static BOOL game_in_progress = NO;


/**
 * Indicate if the user chooses "new" to start a game
 */
static bool new_game = FALSE; ////


#pragma mark Prototypes
static BOOL bigtiles_are_appropriate(void);
static BOOL redraw_for_tiles_or_term0_font(void);
static void wakeup_event_loop(void);
static void hook_plog(const char *str);
static void hook_quit(const char * str);
static NSString *get_lib_directory(void);
#if 0
static NSString *get_doc_directory(void);
#endif
static NSString *AngbandCorrectedDirectoryPath(NSString *originalPath);
static void prepare_paths_and_directories(void);
static void load_prefs(void);
static void init_windows(void);
static BOOL open_game(void); ////half
static void open_tutorial(void); ////half
#if 0
static void play_sound(int event);
#endif
static BOOL check_events(int wait);
/* Used by Angband but not by Sil. */
#if 0
static void cocoa_file_open_hook(const char *path, file_type ftype);
static bool cocoa_get_file(const char *suggested_name, char *path, size_t len);
#endif
static BOOL send_event(NSEvent *event);
static void record_current_savefile(void);

/**
 * Available values for 'wait'
 */
#define CHECK_EVENTS_DRAIN -1
#define CHECK_EVENTS_NO_WAIT	0
#define CHECK_EVENTS_WAIT 1


/**
 * Note when "open"/"new" become valid
 */
static BOOL initialized = NO;

/** Methods for getting the appropriate NSUserDefaults */
@interface NSUserDefaults (AngbandDefaults)
+ (NSUserDefaults *)angbandDefaults;
@end

@implementation NSUserDefaults (AngbandDefaults)
+ (NSUserDefaults *)angbandDefaults
{
    return [NSUserDefaults standardUserDefaults];
}
@end

/**
 * Methods for pulling images out of the Angband bundle (which may be separate
 * from the current bundle in the case of a screensaver
 */
@interface NSImage (AngbandImages)
+ (NSImage *)angbandImage:(NSString *)name;
@end

/** The NSView subclass that draws our Angband image */
@interface AngbandView : NSView
{
    AngbandContext *angbandContext;
}

- (void)setAngbandContext:(AngbandContext *)context;
- (AngbandContext *)angbandContext;

@end

@implementation NSImage (AngbandImages)

/**
 * Returns an image in the resource directoy of the bundle containing the
 * Angband view class.
 */
+ (NSImage *)angbandImage:(NSString *)name
{
    NSBundle *bundle = [NSBundle bundleForClass:[AngbandView class]];
    NSString *path = [bundle pathForImageResource:name];
    return (path) ? [[NSImage alloc] initByReferencingFile:path] : nil;
}

@end


@implementation AngbandContext

- (BOOL)useLiveResizeOptimization
{
    /*
     * If we have graphics turned off, text rendering is fast enough that we
     * don't need to use a live resize optimization.
     */
    return self->inLiveResize && graphics_are_enabled();
}

- (NSSize)baseSize
{
    /*
     * We round the base size down. If we round it up, I believe we may end up
     * with pixels that nobody "owns" that may accumulate garbage. In general
     * rounding down is harmless, because any lost pixels may be sopped up by
     * the border.
     */
    return NSMakeSize(
        floor(self.cols * self.tileSize.width + 2 * self.borderSize.width),
        floor(self.rows * self.tileSize.height + 2 * self.borderSize.height));
}

/** qsort-compatible compare function for CGSizes */
static int compare_advances(const void *ap, const void *bp)
{
    const CGSize *a = ap, *b = bp;
    return (a->width > b->width) - (a->width < b->width);
}

/**
 * Precompute certain metrics (tileSize, fontAscender, fontDescender, nColPre,
 * and nColPost) for the current font.
 */
- (void)updateGlyphInfo
{
    NSFont *screenFont = [self.angbandViewFont screenFont];

    /* Generate a string containing each MacRoman character */
    /*
     * Here and below, dynamically allocate working arrays rather than put them
     * on the stack in case limited stack space is an issue.
     */
    unsigned char *latinString = malloc(GLYPH_COUNT);
    if (latinString == 0) {
        NSException *exc = [NSException exceptionWithName:@"OutOfMemory"
                                        reason:@"latinString in updateGlyphInfo"
                                        userInfo:nil];
        @throw exc;
    }
    size_t i;
    for (i=0; i < GLYPH_COUNT; i++) latinString[i] = (unsigned char)i;

    /* Turn that into unichar. Angband uses ISO Latin 1. */
    NSString *allCharsString = [[NSString alloc] initWithBytes:latinString length:GLYPH_COUNT encoding:NSISOLatin1StringEncoding];
    unichar *unicharString = malloc(GLYPH_COUNT * sizeof(unichar));
    if (unicharString == 0) {
        free(latinString);
        NSException *exc = [NSException exceptionWithName:@"OutOfMemory"
                                        reason:@"unicharString in updateGlyphInfo"
                                        userInfo:nil];
        @throw exc;
    }
    unicharString[0] = 0;
    [allCharsString getCharacters:unicharString range:NSMakeRange(0, MIN(GLYPH_COUNT, [allCharsString length]))];
    allCharsString = nil;
    free(latinString);

    /* Get glyphs */
    CGGlyph *glyphArray = calloc(GLYPH_COUNT, sizeof(CGGlyph));
    if (glyphArray == 0) {
        free(unicharString);
        NSException *exc = [NSException exceptionWithName:@"OutOfMemory"
                                        reason:@"glyphArray in updateGlyphInfo"
                                        userInfo:nil];
        @throw exc;
    }
    CTFontGetGlyphsForCharacters((CTFontRef)screenFont, unicharString,
                                 glyphArray, GLYPH_COUNT);
    free(unicharString);

    /* Get advances. Record the max advance. */
    CGSize *advances = malloc(GLYPH_COUNT * sizeof(CGSize));
    if (advances == 0) {
        free(glyphArray);
        NSException *exc = [NSException exceptionWithName:@"OutOfMemory"
                                        reason:@"advances in updateGlyphInfo"
                                        userInfo:nil];
        @throw exc;
    }
    CTFontGetAdvancesForGlyphs(
        (CTFontRef)screenFont, kCTFontHorizontalOrientation, glyphArray,
        advances, GLYPH_COUNT);
    CGFloat *glyphWidths = malloc(GLYPH_COUNT * sizeof(CGFloat));
    if (glyphWidths == 0) {
        free(glyphArray);
        free(advances);
        NSException *exc = [NSException exceptionWithName:@"OutOfMemory"
                                        reason:@"glyphWidths in updateGlyphInfo"
                                        userInfo:nil];
        @throw exc;
    }
    for (i=0; i < GLYPH_COUNT; i++) {
        glyphWidths[i] = advances[i].width;
    }

    /*
     * For good non-mono-font support, use the median advance. Start by sorting
     * all advances.
     */
    qsort(advances, GLYPH_COUNT, sizeof *advances, compare_advances);

    /* Skip over any initially empty run */
    size_t startIdx;
    for (startIdx = 0; startIdx < GLYPH_COUNT; startIdx++)
    {
        if (advances[startIdx].width > 0) break;
    }

    /* Pick the center to find the median */
    CGFloat medianAdvance = 0;
    /* In case we have all zero advances for some reason */
    if (startIdx < GLYPH_COUNT)
    {
        medianAdvance = advances[(startIdx + GLYPH_COUNT)/2].width;
    }

    free(advances);

    /*
     * Record the ascender and descender.  Some fonts, for instance DIN
     * Condensed and Rockwell in 10.14, the ascent on '@' exceeds that
     * reported by [screenFont ascender].  Get the overall bounding box
     * for the glyphs and use that instead of the ascender and descender
     * values if the bounding box result extends farther from the baseline.
     */
    CGRect bounds = CTFontGetBoundingRectsForGlyphs(
        (CTFontRef) screenFont, kCTFontHorizontalOrientation, glyphArray,
        NULL, GLYPH_COUNT);
    self->_fontAscender = [screenFont ascender];
    if (self->_fontAscender < bounds.origin.y + bounds.size.height) {
        self->_fontAscender = bounds.origin.y + bounds.size.height;
    }
    self->_fontDescender = [screenFont descender];
    if (self->_fontDescender > bounds.origin.y) {
        self->_fontDescender = bounds.origin.y;
    }

    /*
     * Record the tile size.  Round the values (height rounded up; width to
     * nearest unless that would be zero) to have tile boundaries match pixel
     * boundaries.
     */
    if (medianAdvance < 1.0) {
        self->_tileSize.width = 1.0;
    } else {
        self->_tileSize.width = floor(medianAdvance + 0.5);
    }
    self->_tileSize.height = ceil(self.fontAscender - self.fontDescender);

    /*
     * Determine whether neighboring columns need to be redrawn when a
     * character changes.
     */
    CGRect *boxes = malloc(GLYPH_COUNT * sizeof(CGRect));
    if (boxes == 0) {
        free(glyphWidths);
        free(glyphArray);
        NSException *exc = [NSException exceptionWithName:@"OutOfMemory"
                                        reason:@"boxes in updateGlyphInfo"
                                        userInfo:nil];
        @throw exc;
    }
    CGFloat beyond_right = 0.;
    CGFloat beyond_left = 0.;
    CTFontGetBoundingRectsForGlyphs(
        (CTFontRef)screenFont,
        kCTFontHorizontalOrientation,
        glyphArray,
        boxes,
        GLYPH_COUNT);
    for (i = 0; i < GLYPH_COUNT; i++) {
        /* Account for the compression and offset used by drawWChar(). */
        CGFloat compression, offset;
        CGFloat v;

        if (glyphWidths[i] <= self.tileSize.width) {
            compression = 1.;
            offset = 0.5 * (self.tileSize.width - glyphWidths[i]);
        } else {
            compression = self.tileSize.width / glyphWidths[i];
            offset = 0.;
        }
        v = (offset + boxes[i].origin.x) * compression;
        if (beyond_left > v) {
            beyond_left = v;
        }
        v = (offset + boxes[i].origin.x + boxes[i].size.width) * compression;
        if (beyond_right < v) {
            beyond_right = v;
        }
    }
    free(boxes);
    self->_nColPre = ceil(-beyond_left / self.tileSize.width);
    if (beyond_right > self.tileSize.width) {
        self->_nColPost =
            ceil((beyond_right - self.tileSize.width) / self.tileSize.width);
    } else {
        self->_nColPost = 0;
    }

    free(glyphWidths);
    free(glyphArray);
}

- (void)updateImage
{
    NSSize size = NSMakeSize(1, 1);
    
    AngbandView *activeView = [self activeView];
    if (activeView)
    {
        /*
         * If we are in live resize, draw as big as the screen, so we can scale
         * nicely to any size. If we are not in live resize, then use the
         * bounds of the active view.
         */
        NSScreen *screen;
        if ([self useLiveResizeOptimization] && (screen = [[activeView window] screen]) != NULL)
        {
            size = [screen frame].size;
        }
        else
        {
            size = [activeView bounds].size;
        }
    }

    CGLayerRelease(self.angbandLayer);

    /* Use the highest monitor scale factor on the system to work out what
     * scale to draw at - not the recommended method, but works where we
     * can't easily get the monitor the current draw is occurring on. */
    float angbandLayerScale = 1.0;
    if ([[NSScreen mainScreen] respondsToSelector:@selector(backingScaleFactor)]) {
        for (NSScreen *screen in [NSScreen screens]) {
            angbandLayerScale = fmax(angbandLayerScale, [screen backingScaleFactor]);
        }
    }

    /* Make a bitmap context as an example for our layer */
    CGColorSpaceRef cs = CGColorSpaceCreateDeviceRGB();
    CGContextRef exampleCtx = CGBitmapContextCreate(NULL, 1, 1, 8 /* bits per component */, 48 /* bytesPerRow */, cs, kCGImageAlphaNoneSkipFirst | kCGBitmapByteOrder32Host);
    CGColorSpaceRelease(cs);

    /* Create the layer at the appropriate size */
    size.width = fmax(1, ceil(size.width * angbandLayerScale));
    size.height = fmax(1, ceil(size.height * angbandLayerScale));
    self.angbandLayer =
        CGLayerCreateWithContext(exampleCtx, *(CGSize *)&size, NULL);

    CFRelease(exampleCtx);

    /* Set the new context of the layer to draw at the correct scale */
    CGContextRef ctx = CGLayerGetContext(self.angbandLayer);
    CGContextScaleCTM(ctx, angbandLayerScale, angbandLayerScale);

    [self lockFocus];
    [[NSColor blackColor] set];
    NSRectFill((NSRect){NSZeroPoint, [self baseSize]});
    [self unlockFocus];
}

- (void)requestRedraw
{
    if (! self->terminal) return;
    
    term *old = Term;
    
    /* Activate the term */
    Term_activate(self->terminal);
    
    /* Redraw the contents */
    Term_redraw();
    
    /* Flush the output */
    Term_fresh();
    
    /* Restore the old term */
    Term_activate(old);
}

- (void)setTerm:(term *)t
{
    self->terminal = t;
}

- (void)viewWillStartLiveResize:(AngbandView *)view
{
#if USE_LIVE_RESIZE_CACHE
    if (self->inLiveResize < INT_MAX) self->inLiveResize++;
    else [NSException raise:NSInternalInconsistencyException format:@"inLiveResize overflow"];

    if (self->inLiveResize == 1 && graphics_are_enabled())
    {
        [self updateImage];

        [self setNeedsDisplay:YES]; /* We'll need to redisplay everything anyways, so avoid creating all those little redisplay rects */
        [self requestRedraw];
    }
#endif
}

- (void)viewDidEndLiveResize:(AngbandView *)view
{
#if USE_LIVE_RESIZE_CACHE
    if (self->inLiveResize > 0) self->inLiveResize--;
    else [NSException raise:NSInternalInconsistencyException format:@"inLiveResize underflow"];

    if (self->inLiveResize == 0 && graphics_are_enabled())
    {
        [self updateImage];

        [self setNeedsDisplay:YES]; /* We'll need to redisplay everything anyways, so avoid creating all those little redisplay rects */
        [self requestRedraw];
    }
#endif
}

/**
 * If we're trying to limit ourselves to a certain number of frames per second,
 * then compute how long it's been since we last drew, and then wait until the
 * next frame has passed.
 */
- (void)throttle
{
    if (frames_per_second > 0)
    {
        CFAbsoluteTime now = CFAbsoluteTimeGetCurrent();
        CFTimeInterval timeSinceLastRefresh = now - self->lastRefreshTime;
        CFTimeInterval timeUntilNextRefresh = (1. / (double)frames_per_second) - timeSinceLastRefresh;
        
        if (timeUntilNextRefresh > 0)
        {
            usleep((unsigned long)(timeUntilNextRefresh * 1000000.));
        }
    }
    self->lastRefreshTime = CFAbsoluteTimeGetCurrent();
}

- (void)drawWChar:(wchar_t)wchar inRect:(NSRect)tile context:(CGContextRef)ctx
{
    CGFloat tileOffsetY = self.fontAscender;
    CGFloat tileOffsetX = 0.0;
    NSFont *screenFont = [self.angbandViewFont screenFont];
    UniChar unicharString[2] = {(UniChar)wchar, 0};

    /* Get glyph and advance */
    CGGlyph thisGlyphArray[1] = { 0 };
    CGSize advances[1] = { { 0, 0 } };
    CTFontGetGlyphsForCharacters((CTFontRef)screenFont, unicharString, thisGlyphArray, 1);
    CGGlyph glyph = thisGlyphArray[0];
    CTFontGetAdvancesForGlyphs((CTFontRef)screenFont, kCTFontHorizontalOrientation, thisGlyphArray, advances, 1);
    CGSize advance = advances[0];
    
    /*
     * If our font is not monospaced, our tile width is deliberately not big
     * enough for every character. In that event, if our glyph is too wide, we
     * need to compress it horizontally. Compute the compression ratio.
     * 1.0 means no compression.
     */
    double compressionRatio;
    if (advance.width <= NSWidth(tile))
    {
        /* Our glyph fits, so we can just draw it, possibly with an offset */
        compressionRatio = 1.0;
        tileOffsetX = (NSWidth(tile) - advance.width)/2;
    }
    else
    {
        /* Our glyph doesn't fit, so we'll have to compress it */
        compressionRatio = NSWidth(tile) / advance.width;
        tileOffsetX = 0;
    }

    /* Now draw it */
    CGAffineTransform textMatrix = CGContextGetTextMatrix(ctx);
    CGFloat savedA = textMatrix.a;

    /* Set the position */
    textMatrix.tx = tile.origin.x + tileOffsetX;
    textMatrix.ty = tile.origin.y + tileOffsetY;

    /* Maybe squish it horizontally. */
    if (compressionRatio != 1.)
    {
        textMatrix.a *= compressionRatio;
    }

    textMatrix = CGAffineTransformScale( textMatrix, 1.0, -1.0 );
    CGContextSetTextMatrix(ctx, textMatrix);
    CGContextShowGlyphsAtPositions(ctx, &glyph, &CGPointZero, 1);
    
    /* Restore the text matrix if we messed with the compression ratio */
    if (compressionRatio != 1.)
    {
        textMatrix.a = savedA;
        CGContextSetTextMatrix(ctx, textMatrix);
    }

    textMatrix = CGAffineTransformScale( textMatrix, 1.0, -1.0 );
    CGContextSetTextMatrix(ctx, textMatrix);
}

/*
 * Lock and unlock focus on our image or layer, setting up the CTM
 * appropriately.
 */
- (CGContextRef)lockFocusUnscaled
{
    /* Create an NSGraphicsContext representing this CGLayer */
    CGContextRef ctx = CGLayerGetContext(self.angbandLayer);
    NSGraphicsContext *context = [NSGraphicsContext graphicsContextWithGraphicsPort:ctx flipped:NO];
    [NSGraphicsContext saveGraphicsState];
    [NSGraphicsContext setCurrentContext:context];
    CGContextSaveGState(ctx);
    return ctx;
}

- (void)unlockFocus
{
    /* Restore the graphics state */
    CGContextRef ctx = [[NSGraphicsContext currentContext] graphicsPort];
    CGContextRestoreGState(ctx);
    [NSGraphicsContext restoreGraphicsState];
}

- (NSSize)imageSize
{
    /* Return the size of our layer */
    CGSize result = CGLayerGetSize(self.angbandLayer);
    return NSMakeSize(result.width, result.height);
}

- (CGContextRef)lockFocus
{
    return [self lockFocusUnscaled];
}


- (NSRect)rectInImageForTileAtX:(int)x Y:(int)y
{
    int flippedY = y;
    return NSMakeRect(
        x * self.tileSize.width + self.borderSize.width,
        flippedY * self.tileSize.height + self.borderSize.height,
        self.tileSize.width, self.tileSize.height);
}

- (void)setSelectionFont:(NSFont*)font adjustTerminal: (BOOL)adjustTerminal
{
    /* Record the new font */
    self.angbandViewFont = font;

    /* Update our glyph info */
    [self updateGlyphInfo];

    if( adjustTerminal )
    {
        /*
         * Adjust terminal to fit window with new font; save the new columns
         * and rows since they could be changed
         */
        NSRect contentRect =
            [self.primaryWindow
                 contentRectForFrameRect: [self.primaryWindow frame]];

        [self constrainWindowSize:[self terminalIndex]];
        NSSize minSize = self.primaryWindow.contentMinSize;
        NSSize maxSize = self.primaryWindow.contentMaxSize;
        BOOL windowNeedsResizing = NO;
        if (contentRect.size.width < minSize.width) {
            contentRect.size.width = minSize.width;
            windowNeedsResizing = YES;
        } else if (contentRect.size.width > maxSize.width) {
            contentRect.size.width = maxSize.width;
            windowNeedsResizing = YES;
        }
        if (contentRect.size.height < minSize.height) {
            contentRect.size.height = minSize.height;
            windowNeedsResizing = YES;
        } else if (contentRect.size.height > maxSize.height) {
            contentRect.size.height = maxSize.height;
            windowNeedsResizing = YES;
        }
        if (windowNeedsResizing) {
            NSSize size;

            size.width = contentRect.size.width;
            size.height = contentRect.size.height;
            [self.primaryWindow setContentSize:size];
        }
        [self resizeTerminalWithContentRect: contentRect saveToDefaults: YES];
    }

    /* Update our image */
    [self updateImage];
}

- (id)init
{
    if ((self = [super init]))
    {
        /* Default rows and cols */
        self->_cols = 80;
        self->_rows = 24;

        /* Default border size */
        self->_borderSize = NSMakeSize(2, 2);

        /* Allocate our array of views */
        self->_angbandViews = [[NSMutableArray alloc] init];

        self->_nColPre = 0;
        self->_nColPost = 0;

        self->_changes =
            [[PendingTermChanges alloc] initWithColumnsRows:self->_cols
                                        rows:self->_rows];
        self->lastRefreshTime = CFAbsoluteTimeGetCurrent();
        self->inLiveResize = 0;
        self->inFullscreenTransition = NO;

        /* Make the image. Since we have no views, it'll just be a puny 1x1 image. */
        [self updateImage];

        self->_windowVisibilityChecked = NO;
    }
    return self;
}

/**
 * Destroy all the receiver's stuff. This is intended to be callable more than
 * once.
 */
- (void)dispose
{
    self->terminal = NULL;

    /* Disassociate ourselves from our angbandViews */
    [self.angbandViews makeObjectsPerformSelector:@selector(setAngbandContext:) withObject:nil];
    self.angbandViews = nil;

    /* Destroy the layer/image */
    CGLayerRelease(self.angbandLayer);
    self.angbandLayer = NULL;

    /* Font */
    self.angbandViewFont = nil;

    /* Window */
    [self.primaryWindow setDelegate:nil];
    [self.primaryWindow close];
    self.primaryWindow = nil;

    /* Pending changes */
    self.changes = nil;
}

/* Usual Cocoa fare */
- (void)dealloc
{
    [self dispose];
}

- (void)addAngbandView:(AngbandView *)view
{
    if (! [self.angbandViews containsObject:view])
    {
        [self.angbandViews addObject:view];
        [self updateImage];
        [self setNeedsDisplay:YES]; /* We'll need to redisplay everything anyways, so avoid creating all those little redisplay rects */
        [self requestRedraw];
    }
}

/**
 * For defaultFont and setDefaultFont.
 */
static __strong NSFont* gDefaultFont = nil;

+ (NSFont*)defaultFont
{
    return gDefaultFont;
}

+ (void) setDefaultFont:(NSFont*)font
{
    gDefaultFont = font;
}

/**
 * We have this notion of an "active" AngbandView, which is the largest - the
 * idea being that in the screen saver, when the user hits Test in System
 * Preferences, we don't want to keep driving the AngbandView in the
 * background.  Our active AngbandView is the widest - that's a hack all right.
 * Mercifully when we're just playing the game there's only one view.
 */
- (AngbandView *)activeView
{
    if ([self.angbandViews count] == 1)
        return [self.angbandViews objectAtIndex:0];

    AngbandView *result = nil;
    float maxWidth = 0;
    for (AngbandView *angbandView in self.angbandViews)
    {
        float width = [angbandView frame].size.width;
        if (width > maxWidth)
        {
            maxWidth = width;
            result = angbandView;
        }
    }
    return result;
}

- (void)angbandViewDidScale:(AngbandView *)view
{
    /*
     * If we're live-resizing with graphics, we're using the live resize
     * optimization, so don't update the image. Otherwise do it.
     */
    if (! (self->inLiveResize && graphics_are_enabled()) && view == [self activeView])
    {
        [self updateImage];

        [self setNeedsDisplay:YES]; /*we'll need to redisplay everything anyways, so avoid creating all those little redisplay rects */
        [self requestRedraw];
    }
}


- (void)removeAngbandView:(AngbandView *)view
{
    if ([self.angbandViews containsObject:view])
    {
        [self.angbandViews removeObject:view];
        [self updateImage];
        [self setNeedsDisplay:YES]; /* We'll need to redisplay everything anyways, so avoid creating all those little redisplay rects */
        if ([self.angbandViews count]) [self requestRedraw];
    }
}

- (NSWindow *)makePrimaryWindow
{
    if (! self.primaryWindow)
    {
        /*
         * This has to be done after the font is set, which it already is in
         * term_init_cocoa()
         */
        NSSize sz = self.baseSize;
        NSRect contentRect = NSMakeRect( 0.0, 0.0, sz.width, sz.height );

        NSUInteger styleMask = NSTitledWindowMask | NSResizableWindowMask | NSMiniaturizableWindowMask;

        /* Make every window other than the main window closable */
        if ((__bridge AngbandContext*) (angband_term[0]->data) != self)
        {
            styleMask |= NSClosableWindowMask;
        }

        self.primaryWindow =
            [[NSWindow alloc] initWithContentRect:contentRect
                              styleMask:styleMask
                              backing:NSBackingStoreBuffered defer:YES];

        /* Not to be released when closed */
        [self.primaryWindow setReleasedWhenClosed:NO];
        [self.primaryWindow setExcludedFromWindowsMenu: YES]; /* we're using custom window menu handling */

        /* Make the view */
        AngbandView *angbandView = [[AngbandView alloc] initWithFrame:contentRect];
        [angbandView setAngbandContext:self];
        [self.angbandViews addObject:angbandView];
        [self.primaryWindow setContentView:angbandView];

        /* We are its delegate */
        [self.primaryWindow setDelegate:self];

        /*
         * Update our image, since this is probably the first angband view
         * we've gotten.
         */
        [self updateImage];
    }
    return self.primaryWindow;
}



#pragma mark View/Window Passthrough

/**
 * This is what our views call to get us to draw to the window
 */
- (void)drawRect:(NSRect)rect inView:(NSView *)view
{
    /*
     * Take this opportunity to throttle so we don't flush faster than desired.
     */
    BOOL viewInLiveResize = [view inLiveResize];
    if (! viewInLiveResize) [self throttle];

    /* With a GLayer, use CGContextDrawLayerInRect */
    CGContextRef context = [[NSGraphicsContext currentContext] graphicsPort];
    NSRect bounds = [view bounds];
    if (viewInLiveResize) CGContextSetInterpolationQuality(context, kCGInterpolationLow);
    CGContextSetBlendMode(context, kCGBlendModeCopy);
    CGContextDrawLayerInRect(context, *(CGRect *)&bounds, self.angbandLayer);
    if (viewInLiveResize) CGContextSetInterpolationQuality(context, kCGInterpolationDefault);
}

- (BOOL)isOrderedIn
{
    return [[[self.angbandViews lastObject] window] isVisible];
}

- (BOOL)isMainWindow
{
    return [[[self.angbandViews lastObject] window] isMainWindow];
}

- (void)setNeedsDisplay:(BOOL)val
{
    for (NSView *angbandView in self.angbandViews)
    {
        [angbandView setNeedsDisplay:val];
    }
}

- (void)setNeedsDisplayInBaseRect:(NSRect)rect
{
    for (NSView *angbandView in self.angbandViews)
    {
        [angbandView setNeedsDisplayInRect: rect];
    }
}

- (void)displayIfNeeded
{
    [[self activeView] displayIfNeeded];
}

- (int)terminalIndex
{
	int termIndex = 0;

	for( termIndex = 0; termIndex < ANGBAND_TERM_MAX; termIndex++ )
	{
		if( angband_term[termIndex] == self->terminal )
		{
			break;
		}
	}

	return termIndex;
}

- (void)resizeTerminalWithContentRect: (NSRect)contentRect saveToDefaults: (BOOL)saveToDefaults
{
    CGFloat newRows = floor(
        (contentRect.size.height - (self.borderSize.height * 2.0)) /
        self.tileSize.height);
    CGFloat newColumns = ceil(
        (contentRect.size.width - (self.borderSize.width * 2.0)) /
        self.tileSize.width);

    if (newRows < 1 || newColumns < 1) return;
    self.cols = newColumns;
    self.rows = newRows;
    [self.changes resize:self.cols rows:self.rows];

    if( saveToDefaults )
    {
        int termIndex = [self terminalIndex];
        NSArray *terminals = [[NSUserDefaults standardUserDefaults] valueForKey: AngbandTerminalsDefaultsKey];

        if( termIndex < (int)[terminals count] )
        {
            NSMutableDictionary *mutableTerm = [[NSMutableDictionary alloc] initWithDictionary: [terminals objectAtIndex: termIndex]];
            [mutableTerm setValue: [NSNumber numberWithInteger: self.cols]
                         forKey: AngbandTerminalColumnsDefaultsKey];
            [mutableTerm setValue: [NSNumber numberWithInteger: self.rows]
                         forKey: AngbandTerminalRowsDefaultsKey];

            NSMutableArray *mutableTerminals = [[NSMutableArray alloc] initWithArray: terminals];
            [mutableTerminals replaceObjectAtIndex: termIndex withObject: mutableTerm];

            [[NSUserDefaults standardUserDefaults] setValue: mutableTerminals forKey: AngbandTerminalsDefaultsKey];
        }
    }

    term *old = Term;
    Term_activate( self->terminal );
    Term_resize( (int)newColumns, (int)newRows);
    Term_redraw();
    Term_activate( old );
}

- (void)constrainWindowSize:(int)termIdx
{
    NSSize minsize, maxsize;

    if (termIdx == 0) {
       minsize.width = 80;
       minsize.height = 24;
    } else {
       minsize.width = 1;
       minsize.height = 1;
    }
    minsize.width =
        minsize.width * self.tileSize.width + self.borderSize.width * 2.0;
    minsize.height =
        minsize.height * self.tileSize.height + self.borderSize.height * 2.0;
    [[self makePrimaryWindow] setContentMinSize:minsize];
    /*
     * Unsigned 8-bit integers are used to store the number of rows and
     * columns for a terminal, so do not allow the size to exceed those limits.
     */
    maxsize.width = 255 * self.tileSize.width + self.borderSize.width * 2.0;
    maxsize.height = 255 * self.tileSize.height + self.borderSize.height * 2.0;
    [[self makePrimaryWindow] setContentMaxSize:maxsize];
    self.primaryWindow.contentResizeIncrements = self.tileSize;
}

- (void)saveWindowVisibleToDefaults: (BOOL)windowVisible
{
	int termIndex = [self terminalIndex];
	BOOL safeVisibility = (termIndex == 0) ? YES : windowVisible; /* Ensure main term doesn't go away because of these defaults */
	NSArray *terminals = [[NSUserDefaults standardUserDefaults] valueForKey: AngbandTerminalsDefaultsKey];

	if( termIndex < (int)[terminals count] )
	{
		NSMutableDictionary *mutableTerm = [[NSMutableDictionary alloc] initWithDictionary: [terminals objectAtIndex: termIndex]];
		[mutableTerm setValue: [NSNumber numberWithBool: safeVisibility] forKey: AngbandTerminalVisibleDefaultsKey];

		NSMutableArray *mutableTerminals = [[NSMutableArray alloc] initWithArray: terminals];
		[mutableTerminals replaceObjectAtIndex: termIndex withObject: mutableTerm];

		[[NSUserDefaults standardUserDefaults] setValue: mutableTerminals forKey: AngbandTerminalsDefaultsKey];
	}
}

- (BOOL)windowVisibleUsingDefaults
{
	int termIndex = [self terminalIndex];

	if( termIndex == 0 )
	{
		return YES;
	}

	NSArray *terminals = [[NSUserDefaults standardUserDefaults] valueForKey: AngbandTerminalsDefaultsKey];
	BOOL visible = NO;

	if( termIndex < (int)[terminals count] )
	{
		NSDictionary *term = [terminals objectAtIndex: termIndex];
		NSNumber *visibleValue = [term valueForKey: AngbandTerminalVisibleDefaultsKey];

		if( visibleValue != nil )
		{
			visible = [visibleValue boolValue];
		}
	}

	return visible;
}

#pragma mark -
#pragma mark NSWindowDelegate Methods

/*- (void)windowWillStartLiveResize: (NSNotification *)notification
{
}*/

- (void)windowDidEndLiveResize: (NSNotification *)notification
{
    NSWindow *window = [notification object];
    NSRect contentRect = [window contentRectForFrameRect: [window frame]];
    [self resizeTerminalWithContentRect: contentRect saveToDefaults: !(self->inFullscreenTransition)];
}

/*- (NSSize)windowWillResize: (NSWindow *)sender toSize: (NSSize)frameSize
{
} */

- (void)windowWillEnterFullScreen: (NSNotification *)notification
{
    self->inFullscreenTransition = YES;
}

- (void)windowDidEnterFullScreen: (NSNotification *)notification
{
    NSWindow *window = [notification object];
    NSRect contentRect = [window contentRectForFrameRect: [window frame]];
    self->inFullscreenTransition = NO;
    [self resizeTerminalWithContentRect: contentRect saveToDefaults: NO];
}

- (void)windowWillExitFullScreen: (NSNotification *)notification
{
    self->inFullscreenTransition = YES;
}

- (void)windowDidExitFullScreen: (NSNotification *)notification
{
    NSWindow *window = [notification object];
    NSRect contentRect = [window contentRectForFrameRect: [window frame]];
    self->inFullscreenTransition = NO;
    [self resizeTerminalWithContentRect: contentRect saveToDefaults: NO];
}

- (void)windowDidBecomeMain:(NSNotification *)notification
{
    NSWindow *window = [notification object];

    if( window != self.primaryWindow )
    {
        return;
    }

    int termIndex = [self terminalIndex];
    NSMenuItem *item = [[[NSApplication sharedApplication] windowsMenu] itemWithTag: AngbandWindowMenuItemTagBase + termIndex];
    [item setState: NSOnState];

    if( [[NSFontPanel sharedFontPanel] isVisible] )
    {
        [[NSFontPanel sharedFontPanel] setPanelFont:self.angbandViewFont
                                       isMultiple: NO];
    }
}

- (void)windowDidResignMain: (NSNotification *)notification
{
    NSWindow *window = [notification object];

    if( window != self.primaryWindow )
    {
        return;
    }

    int termIndex = [self terminalIndex];
    NSMenuItem *item = [[[NSApplication sharedApplication] windowsMenu] itemWithTag: AngbandWindowMenuItemTagBase + termIndex];
    [item setState: NSOffState];
}

- (void)windowWillClose: (NSNotification *)notification
{
    /*
     * If closing only because the application is terminating, don't update
     * the visible state for when the application is relaunched.
     */
    if (! quit_when_ready) {
        [self saveWindowVisibleToDefaults: NO];
    }
}

@end


@implementation AngbandView

- (BOOL)isOpaque
{
    return YES;
}

- (BOOL)isFlipped
{
    return YES;
}

- (void)drawRect:(NSRect)rect
{
    if (! angbandContext)
    {
        /* Draw bright orange, 'cause this ain't right */
        [[NSColor orangeColor] set];
        NSRectFill([self bounds]);
    }
    else
    {
        /* Tell the Angband context to draw into us */
        [angbandContext drawRect:rect inView:self];
    }
}

- (void)setAngbandContext:(AngbandContext *)context
{
    angbandContext = context;
}

- (AngbandContext *)angbandContext
{
    return angbandContext;
}

- (void)setFrameSize:(NSSize)size
{
    BOOL changed = ! NSEqualSizes(size, [self frame].size);
    [super setFrameSize:size];
    if (changed) [angbandContext angbandViewDidScale:self];
}

- (void)viewWillStartLiveResize
{
    [angbandContext viewWillStartLiveResize:self];
}

- (void)viewDidEndLiveResize
{
    [angbandContext viewDidEndLiveResize:self];
}

@end



/**
 * ------------------------------------------------------------------------
 * Some generic functions
 * ------------------------------------------------------------------------ */

/**
 * Given an Angband color index, returns the index into angband_color_table
 * to be used as the background color.  The returned index is between -1 and
 * MAX_COLORS - 1 inclusive where -1 means use the RGB triplet, (0, 0, 0).
 */
static int get_background_color_index(int idx)
{
    /*
     * The Cocoa interface has always been using (0,0,0) as the clear color
     * and not angband_color_table[0].  Sil-q's main-win.c uses both ((0,0,0)
     * in Term_xtra_win_clear() and Term_wipe_win() and win_clr[0], which is
     * set from angband_color_table[0], in Term_text_win() for the BG_BLACK
     * case).
     */
    int ibkg = -1;

    switch (idx / MAX_COLORS) {
    case BG_BLACK:
        /* There's nothing to do. */
        break;

    case BG_SAME:
        ibkg = idx % MAX_COLORS;
        break;

    case BG_DARK:
        ibkg = 16;
        break;

    default:
        assert(0);
    }

    return ibkg;
}


/**
 * Sets an Angband color at a given index
 */
static void set_color_for_index(int idx)
{
    u16b rv, gv, bv;
    
    /* Extract the R,G,B data */
    rv = angband_color_table[idx][1];
    gv = angband_color_table[idx][2];
    bv = angband_color_table[idx][3];
    
    CGContextSetRGBFillColor([[NSGraphicsContext currentContext] graphicsPort], rv/255., gv/255., bv/255., 1.);
}

/**
 * Remember the current character in UserDefaults so we can select it by
 * default next time.
 */
static void record_current_savefile(void)
{
    NSString *savefileString = [[NSString stringWithCString:savefile encoding:NSMacOSRomanStringEncoding] lastPathComponent];
    if (savefileString)
    {
        NSUserDefaults *angbandDefs = [NSUserDefaults angbandDefaults];
        [angbandDefs setObject:savefileString forKey:@"SaveFile"];
    }
}


/**
 * ------------------------------------------------------------------------
 * Support for the "z-term.c" package
 * ------------------------------------------------------------------------
 */


/**
 * Initialize a new Term
 */
static void Term_init_cocoa(term *t)
{
    @autoreleasepool {
        NSUserDefaults *defs = [NSUserDefaults angbandDefaults];
        AngbandContext *context = [[AngbandContext alloc] init];

        /* Give the term ownership of the context */
        t->data = (void *)CFBridgingRetain(context);

        /* Handle graphics */
        t->higher_pict = !! use_graphics;
        t->always_pict = FALSE;

        NSDisableScreenUpdates();

        /*
         * Figure out the frame autosave name based on the index of this term
         */
        NSString *autosaveName = nil;
        int termIdx;
        for (termIdx = 0; termIdx < ANGBAND_TERM_MAX; termIdx++)
        {
            if (angband_term[termIdx] == t)
            {
                autosaveName =
                    [NSString stringWithFormat:@"AngbandTerm-%d", termIdx];
                break;
            }
        }

        /* Set its font. */
        NSString *fontName =
            [defs
                stringForKey:[NSString stringWithFormat:@"FontName-%d", termIdx]];
        if (! fontName) fontName = [[AngbandContext defaultFont] fontName];

        /*
         * Use a smaller default font for the other windows, but only if the
         * font hasn't been explicitly set.
         */
        float fontSize = (termIdx == WINDOW_MONSTER) ?
            FallbackFontSizeMonster : ((termIdx > 0) ?
            FallbackFontSizeSub : [[AngbandContext defaultFont] pointSize]);

        NSNumber *fontSizeNumber =
           [defs
               valueForKey: [NSString stringWithFormat: @"FontSize-%d", termIdx]];

        if( fontSizeNumber != nil )
        {
            fontSize = [fontSizeNumber floatValue];
        }

        NSFont *newFont = [NSFont fontWithName:fontName size:fontSize];
        if (!newFont) {
            float fallbackSize = (termIdx == WINDOW_MONSTER) ?
                FallbackFontSizeMonster : ((termIdx > 0) ?
                FallbackFontSizeSub : FallbackFontSizeMain);

            newFont = [NSFont fontWithName:FallbackFontName size:fallbackSize];
            if (!newFont) {
                newFont = [NSFont systemFontOfSize:fallbackSize];
                if (!newFont) {
                    newFont = [NSFont systemFontOfSize:0.0];
                }
            }
            /* Override the bad preferences. */
            [defs setValue:[newFont fontName]
                forKey:[NSString stringWithFormat:@"FontName-%d", termIdx]];
            [defs setFloat:[newFont pointSize]
                forKey:[NSString stringWithFormat:@"FontSize-%d", termIdx]];
        }
        [context setSelectionFont:newFont adjustTerminal: NO];

        NSArray *terminalDefaults =
            [[NSUserDefaults standardUserDefaults]
                valueForKey: AngbandTerminalsDefaultsKey];
        NSInteger rows = 24;
        NSInteger columns = 80;

        if( termIdx < (int)[terminalDefaults count] )
        {
            NSDictionary *term = [terminalDefaults objectAtIndex: termIdx];
            rows = [[term valueForKey: AngbandTerminalRowsDefaultsKey] integerValue];
            columns = [[term valueForKey: AngbandTerminalColumnsDefaultsKey] integerValue];
        }

        context.cols = columns;
        context.rows = rows;
        [context.changes resize:columns rows:rows];

        /* Get the window */
        NSWindow *window = [context makePrimaryWindow];
    
        /* Set its title and, for auxiliary terms, tentative size */
        if (termIdx == 0)
        {
            [window setTitle:@"Sil"]; ////half
        }
        else
        {
            [window setTitle:[NSString stringWithUTF8String: angband_term_name[termIdx]]];
        }
        [context constrainWindowSize:termIdx];

        /*
         * If this is the first term, and we support full screen (Mac OS X Lion
         * or later), then allow it to go full screen (sweet). Allow other
         * terms to be FullScreenAuxilliary, so they can at least show up.
         * Unfortunately in Lion they don't get brought to the full screen
         * space; but they would only make sense on multiple displays anyways
         * so it's not a big loss.
         */
        if ([window respondsToSelector:@selector(toggleFullScreen:)])
        {
            NSWindowCollectionBehavior behavior = [window collectionBehavior];
            behavior |=
                (termIdx == 0 ?
                 Angband_NSWindowCollectionBehaviorFullScreenPrimary :
                 Angband_NSWindowCollectionBehaviorFullScreenAuxiliary);
            [window setCollectionBehavior:behavior];
        }
    
        /* No Resume support yet, though it would not be hard to add */
        if ([window respondsToSelector:@selector(setRestorable:)])
        {
            [window setRestorable:NO];
        }

        /* default window placement */
        {
            ////half: commented out this block to replace it with my defaults
#if 0
            static NSRect overallBoundingRect;

            if( termIdx == 0 )
            {
                /*
                 * This is a bit of a trick to allow us to display multiple
                 * windows in the "standard default" window position in OS X:
                 * the upper center of the screen.  The term sizes set in
                 * load_prefs() are based on a 5-wide by 3-high grid, with the
                 * main term being 4/5 wide by 2/3 high (hence the scaling to
                 * find what the containing rect would be).
                 */
                NSRect originalMainTermFrame = [window frame];
                NSRect scaledFrame = originalMainTermFrame;
                scaledFrame.size.width *= 5.0 / 4.0;
                scaledFrame.size.height *= 3.0 / 2.0;
                scaledFrame.size.width += 1.0; /* spacing between window columns */
                scaledFrame.size.height += 1.0; /* spacing between window rows */
                [window setFrame: scaledFrame  display: NO];
                [window center];
                overallBoundingRect = [window frame];
                [window setFrame: originalMainTermFrame display: NO];
            }

            static NSRect mainTermBaseRect;
            NSRect windowFrame = [window frame];

            if( termIdx == 0 )
            {
                /*
                 * The height and width adjustments were determined
                 * experimentally, so that the rest of the windows line up
                 * nicely without overlapping.
                 */
                windowFrame.size.width += 7.0;
                windowFrame.size.height += 9.0;
                windowFrame.origin.x = NSMinX( overallBoundingRect );
                windowFrame.origin.y =
                     NSMaxY( overallBoundingRect ) - NSHeight( windowFrame );
                mainTermBaseRect = windowFrame;
            }
            else if( termIdx == 1 )
            {
                windowFrame.origin.x = NSMinX( mainTermBaseRect );
                windowFrame.origin.y =
                    NSMinY( mainTermBaseRect ) - NSHeight( windowFrame ) - 1.0;
            }
            else if( termIdx == 2 )
            {
                windowFrame.origin.x = NSMaxX( mainTermBaseRect ) + 1.0;
                windowFrame.origin.y =
                    NSMaxY( mainTermBaseRect ) - NSHeight( windowFrame );
            }
            else if( termIdx == 3 )
            {
                windowFrame.origin.x = NSMaxX( mainTermBaseRect ) + 1.0;
                windowFrame.origin.y =
                    NSMinY( mainTermBaseRect ) - NSHeight( windowFrame ) - 1.0;
            }
            else if( termIdx == 4 )
            {
                windowFrame.origin.x = NSMaxX( mainTermBaseRect ) + 1.0;
                windowFrame.origin.y = NSMinY( mainTermBaseRect );
            }
            else if( termIdx == 5 )
            {
                windowFrame.origin.x =
                    NSMinX( mainTermBaseRect ) + NSWidth( windowFrame ) + 1.0;
                windowFrame.origin.y =
                    NSMinY( mainTermBaseRect ) - NSHeight( windowFrame ) - 1.0;
            }
#endif

            ////half: here are the new window placement defaults
            NSRect windowFrame = [window frame];

            NSRect screen = [[NSScreen mainScreen] frame];

            int left = 45;
            int bottom = (int)screen.size.height - 795; //285;
            int main_w = 808;
            int main_h = 600;
            int recall_h = 173;
            int side_w = 425;
            int inventory_h = 310;
            int equipment_h = 207;
            int combat_rolls_h = 256;

            if( termIdx == WINDOW_MAIN )
            {
                windowFrame.size.width = main_w;
                windowFrame.size.height = main_h;
                windowFrame.origin.x = left;
                windowFrame.origin.y = bottom + recall_h;
            }
            else if( termIdx == WINDOW_INVEN )
            {
                windowFrame.size.width = side_w;
                windowFrame.size.height = inventory_h;
                windowFrame.origin.x = left + main_w + 2;
                windowFrame.origin.y = bottom + combat_rolls_h + equipment_h;
            }
            else if( termIdx == WINDOW_EQUIP )
            {
                windowFrame.size.width = side_w;
                windowFrame.size.height = equipment_h;
                windowFrame.origin.x = left + main_w + 2;
                windowFrame.origin.y = bottom + combat_rolls_h;
            }
            else if( termIdx == WINDOW_COMBAT_ROLLS )
            {
                windowFrame.size.width = side_w;
                windowFrame.size.height = combat_rolls_h;
                windowFrame.origin.x = left + main_w + 2;
                windowFrame.origin.y = bottom;
            }
            else if( termIdx == WINDOW_MONSTER )
            {
                windowFrame.size.width = main_w;
                windowFrame.size.height = recall_h;
                windowFrame.origin.x = left;
                windowFrame.origin.y = bottom;
            }
            else
            {
                windowFrame.size.width = side_w;
                windowFrame.size.height = inventory_h;
                windowFrame.origin.x = left + (termIdx * 10) - 40;
                windowFrame.origin.y = bottom + recall_h + 200 - (termIdx * 10) + 40;
            }
            ////half: this ends the new window placement defaults

            [window setFrame: windowFrame display: NO];
        }

        /*
         * Override the default frame above if the user has adjusted windows in
         * the past
         */
        if (autosaveName) [window setFrameAutosaveName:autosaveName];

        /*
         * Tell it about its term. Do this after we've sized it so that the
         * sizing doesn't trigger redrawing and such.
         */
        [context setTerm:t];

        /*
         * Only order front if it's the first term. Other terms will be ordered
         * front from AngbandUpdateWindowVisibility(). This is to work around a
         * problem where Angband aggressively tells us to initialize terms that
         * don't do anything!
         */
        if (t == angband_term[0])
            [context.primaryWindow makeKeyAndOrderFront: nil];

        NSEnableScreenUpdates();

        /* Set "mapped" flag */
        t->mapped_flag = true;
    }
}



/**
 * Nuke an old Term
 */
static void Term_nuke_cocoa(term *t)
{
    @autoreleasepool {
        AngbandContext *context = (__bridge AngbandContext*) t->data;
        if (context)
        {
            /* Tell the context to get rid of its windows, etc. */
            [context dispose];

            /* Balance our CFBridgingRetain from when we created it */
            CFRelease(t->data);
        
            /* Done with it */
            t->data = NULL;
        }
    }
}

/**
 * Returns the CGImageRef corresponding to an image with the given path.
 * Transfers ownership to the caller.
 */
static CGImageRef create_angband_image(NSString *path)
{
    CGImageRef decodedImage = NULL, result = NULL;
    
    /* Try using ImageIO to load the image */
    if (path)
    {
        NSURL *url = [[NSURL alloc] initFileURLWithPath:path isDirectory:NO];
        if (url)
        {
            NSDictionary *options = [[NSDictionary alloc] initWithObjectsAndKeys:(id)kCFBooleanTrue, kCGImageSourceShouldCache, nil];
            CGImageSourceRef source = CGImageSourceCreateWithURL((CFURLRef)url, (CFDictionaryRef)options);
            if (source)
            {
                /*
                 * We really want the largest image, but in practice there's
                 * only going to be one
                 */
                decodedImage = CGImageSourceCreateImageAtIndex(source, 0, (CFDictionaryRef)options);
                CFRelease(source);
            }
        }
    }
    
    /*
     * Draw the sucker to defeat ImageIO's weird desire to cache and decode on
     * demand. Our images aren't that big!
     */
    if (decodedImage)
    {
        size_t width = CGImageGetWidth(decodedImage), height = CGImageGetHeight(decodedImage);
        
        /* Compute our own bitmap info */
        CGBitmapInfo imageBitmapInfo = CGImageGetBitmapInfo(decodedImage);
        CGBitmapInfo contextBitmapInfo = kCGBitmapByteOrderDefault;
        
        switch (imageBitmapInfo & kCGBitmapAlphaInfoMask) {
            case kCGImageAlphaNone:
            case kCGImageAlphaNoneSkipLast:
            case kCGImageAlphaNoneSkipFirst:
                /* No alpha */
                contextBitmapInfo |= kCGImageAlphaNone;
                break;
            default:
                /* Some alpha, use premultiplied last which is most efficient. */
                contextBitmapInfo |= kCGImageAlphaPremultipliedLast;
                break;
        }

        /* Draw the source image flipped, since the view is flipped */
        CGContextRef ctx = CGBitmapContextCreate(NULL, width, height, CGImageGetBitsPerComponent(decodedImage), CGImageGetBytesPerRow(decodedImage), CGImageGetColorSpace(decodedImage), contextBitmapInfo);
        if (ctx) {
            CGContextSetBlendMode(ctx, kCGBlendModeCopy);
            CGContextTranslateCTM(ctx, 0.0, height);
            CGContextScaleCTM(ctx, 1.0, -1.0);
            CGContextDrawImage(ctx, CGRectMake(0, 0, width, height), decodedImage);
            result = CGBitmapContextCreateImage(ctx);
            CFRelease(ctx);
        }

        CGImageRelease(decodedImage);
    }
    return result;
}

/**
 * React to changes
 */
static errr Term_xtra_cocoa_react(void)
{
    /* Don't actually switch graphics until the game is running */
    if (!initialized || !game_in_progress) return (-1);

    @autoreleasepool {
        /* Handle graphics */
        if (use_graphics != arg_graphics) {
            /* Get rid of the old image. CGImageRelease is NULL-safe. */
            CGImageRelease(pict_image);
            pict_image = NULL;

            /* Try creating the image if we want one */
            if (arg_graphics == GRAPHICS_MICROCHASM)
            {
                NSString *img_path = [NSString
                    stringWithFormat:@"%s/graf/16x16_microchasm.png", ANGBAND_DIR_XTRA];
                pict_image = create_angband_image(img_path);

                /* If we failed to create the image, revert to ASCII. */
                if (! pict_image) {
                    arg_graphics = GRAPHICS_NONE;
                    [[NSUserDefaults angbandDefaults]
                        setInteger:GRAPHICS_NONE
                        forKey:AngbandGraphicsDefaultsKey];

                    NSAlert *alert = [[NSAlert alloc] init];
                    alert.messageText = @"Failed to Load Tile Set";
                    alert.informativeText =
                        @"Could not load the tile set.  Switching back to ASCII.";
                    [alert runModal];
                } else {
                    pict_cell_width = 16;
                    pict_cell_height = 16;
                }
            } else if (arg_graphics != GRAPHICS_NONE) {
                arg_graphics = GRAPHICS_NONE;
                [[NSUserDefaults angbandDefaults]
                    setInteger:GRAPHICS_NONE
                    forKey:AngbandGraphicsDefaultsKey];

                NSAlert *alert = [[NSAlert alloc] init];
                alert.messageText = @"Unknown Tile Set";
                alert.informativeText =
                    @"That's an unrecognized tile set.  Switching back to ASCII.";
                [alert runModal];
            }

            /* Record what we did */
            use_graphics = arg_graphics;
            if (use_graphics) {
                use_transparency = TRUE;
                ANGBAND_GRAF = "new";
                if (bigtiles_are_appropriate()) {
                    if (!use_bigtile) {
                        use_bigtile = TRUE;
                        tile_multipliers_changed = 1;
                    }
                } else {
                    if (use_bigtile) {
                        use_bigtile = FALSE;
                        tile_multipliers_changed = 1;
                    }
                }
            } else {
                use_transparency = FALSE;
                ANGBAND_GRAF = "old";
                if (use_bigtile) {
                    use_bigtile = FALSE;
                    tile_multipliers_changed = 1;
                }
            }

            /* Enable or disable higher picts. */
            for (int iterm = 0; iterm < ANGBAND_TERM_MAX; ++iterm) {
                if (angband_term[iterm]) {
                    angband_term[iterm]->higher_pict = !! use_graphics;
                }
            }

            if (pict_image && use_graphics)
            {
                /*
                 * Compute the row and column count via the image height and
                 * width.
                 */
                pict_rows = (int)(CGImageGetHeight(pict_image) /
                                  pict_cell_height);
                pict_cols = (int)(CGImageGetWidth(pict_image) /
                                  pict_cell_width);
            }
            else
            {
                pict_rows = 0;
                pict_cols = 0;
            }

            /* Reset visuals */
            if (! tile_multipliers_changed)
            {
                reset_visuals(TRUE);
            }
        }

        if (tile_multipliers_changed) {
            /* Reset visuals */
            reset_visuals(TRUE);

            if (character_dungeon) {
                /*
                 * Reset the panel.  Only do so if have a dungeon; otherwise
                 * can see crashes if changing graphics or the font before or
                 * during character generation.
                 */
                verify_panel();
            }

            tile_multipliers_changed = 0;
        }
    }

    /* Success */
    return (0);
}


/**
 * Draws one tile as a helper function for Term_xtra_cocoa_fresh().
 */
static void draw_image_tile(
    NSGraphicsContext* nsContext,
    CGContextRef cgContext,
    CGImageRef image,
    NSRect srcRect,
    NSRect dstRect,
    NSCompositingOperation op)
{
    /* Flip the source rect since the source image is flipped */
    CGAffineTransform flip = CGAffineTransformIdentity;
    flip = CGAffineTransformTranslate(flip, 0.0, CGImageGetHeight(image));
    flip = CGAffineTransformScale(flip, 1.0, -1.0);
    CGRect flippedSourceRect =
        CGRectApplyAffineTransform(NSRectToCGRect(srcRect), flip);

    /*
     * When we use high-quality resampling to draw a tile, pixels from outside
     * the tile may bleed in, causing graphics artifacts.  Work around that.
     */
    CGImageRef subimage =
        CGImageCreateWithImageInRect(image, flippedSourceRect);
    [nsContext setCompositingOperation:op];
    CGContextDrawImage(cgContext, NSRectToCGRect(dstRect), subimage);
    CGImageRelease(subimage);
}


/**
 * This is a helper function for Term_xtra_cocoa_fresh():  look before a block
 * of text on a row to see if the bounds for rendering and clipping need to be
 * extended.
 */
static void query_before_text(
    PendingTermChanges *tc, int iy, int npre, int* pclip, int* prend)
{
    int start = *prend;
    int i = start - 1;

    while (1) {
        if (i < 0 || i < start - npre) {
            break;
        }
        enum PendingCellChangeType ctype = [tc getCellChangeType:i row:iy];

        if (ctype == CELL_CHANGE_TILE) {
            /*
             * The cell has been rendered with a tile.  Do not want to modify
             * its contents so the clipping and rendering region can not be
             * extended.
             */
            break;
        } else if (ctype == CELL_CHANGE_NONE) {
            /*
             * It has not changed (or using big tile mode and it is within
             * a changed tile but is not the left cell for that tile) so
             * inquire what it is.
             */
            byte_hack a[2];
            char c[2];

            Term_what(i, iy, a + 1, c + 1);
            if (use_graphics && (a[1] & 0x80) && (c[1] & 0x80)) {
                /*
                 * It is an unchanged location rendered with a tile.  Do not
                 * want to modify its contents so the clipping and rendering
                 * region can not be extended.
                 */
                break;
            }
            if (use_bigtile && i > 0) {
                Term_what(i - 1, iy, a, c);
                if (use_graphics && (a[0] & 0x80) && (c[0] & 0x80)) {
                    /*
                     * It is the right cell of a location rendered with a tile.
                     * Do not want to modify its contents so the clipping and
                     * rendering region can not be extended.
                     */
                    break;
                }
            }
            /*
             * It is unchanged text.  A character from the changed region
             * may have extended into it so render it to clear that.
             */
            [tc markTextChange:i row:iy glyph:c[1] color:a[1]];
            *pclip = i;
            *prend = i;
            --i;
        } else {
            /*
             * The cell has been wiped or had changed text rendered.  Do
             * not need to render.  Can extend the clipping rectangle into it.
             */
            *pclip = i;
            --i;
        }
    }
}


/**
 * This is a helper function for Term_xtra_cocoa_fresh():  look after a block
 * of text on a row to see if the bounds for rendering and clipping need to be
 * extended.
 */
static void query_after_text(
    PendingTermChanges *tc, int iy, int npost, int *pclip, int *prend)
{
    int end = *prend;
    int i = end + 1;
    int ncol = tc.columnCount;

    while (1) {
        if (i >= ncol) {
            break;
        }

        enum PendingCellChangeType ctype = [tc getCellChangeType:i row:iy];

        /*
         * Be willing to consolidate this block with the one after it.  This
         * logic should be sufficient to avoid redraws of the region between
         * changed blocks of text if angbandContext.nColPre is zero or one.
         * For larger values of nColPre, would need to do something more to
         * avoid extra redraws.
         */
        if (i > end + npost && ctype != CELL_CHANGE_TEXT
                && ctype != CELL_CHANGE_WIPE) {
            break;
        }

        if (ctype == CELL_CHANGE_TILE) {
            /*
             * The cell has been rendered with a tile.  Do not want to modify
             * its contents so the clipping and rendering region can not be
             * extended.
             */
            break;
        } else if (ctype == CELL_CHANGE_NONE) {
            /* It has not changed so inquire what it is. */
            byte_hack a;
            char c;

            Term_what(i, iy, &a, &c);
            if (use_graphics && (a & 0x80) && (c & 0x80)) {
                /*
                 * It is an unchanged location rendered with a tile.  Do not
                 * want to modify its contents so the clipping and rendering
                 * region can not be extended.
                 */
                break;
            }
            /*
             * It is unchanged text.  A character from the changed region
             * may have extended into it so render it to clear that.
             */
            [tc markTextChange:i row:iy glyph:c color:a];
            *pclip = i;
            *prend = i;
            ++i;
        } else {
            /*
             * Have come to another region of changed text or another region
             * to wipe.  Combine the regions to minimize redraws.
             */
            *pclip = i;
            *prend = i;
            end = i;
            ++i;
        }
    }
}


/**
 * Draw the pending changes saved in angbandContext->changes.
 */
static void Term_xtra_cocoa_fresh(AngbandContext* angbandContext)
{
    int graf_width, graf_height, alphablend;

    if (angbandContext.changes.hasTileChanges) {
        CGImageAlphaInfo ainfo = CGImageGetAlphaInfo(pict_image);

        graf_width = pict_cell_width;
        graf_height = pict_cell_height;
        alphablend = (ainfo & (kCGImageAlphaPremultipliedFirst |
            kCGImageAlphaPremultipliedLast)) ? 1 : 0;
    } else {
        graf_width = 0;
        graf_height = 0;
        alphablend = 0;
    }

    CGContextRef ctx = [angbandContext lockFocus];

    if (angbandContext.changes.hasTextChanges
            || angbandContext.changes.hasWipeChanges) {
        NSFont *selectionFont = [angbandContext.angbandViewFont screenFont];
        [selectionFont set];
    }

    for (int iy = angbandContext.changes.firstChangedRow;
            iy <= angbandContext.changes.lastChangedRow;
            ++iy) {
        /* Skip untouched rows. */
        if ([angbandContext.changes getFirstChangedColumnInRow:iy]
                > [angbandContext.changes getLastChangedColumnInRow:iy]) {
            continue;
        }
        int ix = [angbandContext.changes getFirstChangedColumnInRow:iy];
        int ixmax = [angbandContext.changes getLastChangedColumnInRow:iy];

        while (1) {
            int jx;

            if (ix > ixmax) {
                break;
            }

            switch ([angbandContext.changes getCellChangeType:ix row:iy]) {
            case CELL_CHANGE_NONE:
                ++ix;
                break;

            case CELL_CHANGE_TILE:
                {
                    /*
                     * Because changes are made to the compositing mode, save
                     * the incoming value.
                     */
                    NSGraphicsContext *nsContext =
                        [NSGraphicsContext currentContext];
                    NSCompositingOperation op = nsContext.compositingOperation;
                    int step = (use_bigtile) ? 2 : 1;

                    jx = ix;
                    while (jx <= ixmax
                            && [angbandContext.changes getCellChangeType:jx row:iy]
                            == CELL_CHANGE_TILE) {
                        NSRect destinationRect =
                            [angbandContext rectInImageForTileAtX:jx Y:iy];
                        struct PendingTileChange tileIndices =
                            [angbandContext.changes
                                           getCellTileChange:jx row:iy];
                        NSRect sourceRect, terrainRect;

                        destinationRect.size.width *= step;
                        sourceRect.origin.x = graf_width * tileIndices.fgdCol;
                        sourceRect.origin.y = graf_height * tileIndices.fgdRow;
                        sourceRect.size.width = graf_width;
                        sourceRect.size.height = graf_height;
                        terrainRect.origin.x = graf_width * tileIndices.bckCol;
                        terrainRect.origin.y = graf_height *
                            tileIndices.bckRow;
                        terrainRect.size.width = graf_width;
                        terrainRect.size.height = graf_height;
                        if (alphablend) {
                            BOOL alert = tileIndices.mask & PICT_MASK_ALERT;
                            BOOL glow = tileIndices.mask & PICT_MASK_GLOW;

                            draw_image_tile(
                                nsContext,
                                ctx,
                                pict_image,
                                terrainRect,
                                destinationRect,
                                NSCompositeCopy);
                            if (glow) {
                                NSRect glowRect;

                                glowRect.origin.x = graf_width *
                                    (0x7F & misc_to_char[ICON_GLOW]);
                                glowRect.origin.y = graf_height *
                                    (0x7F & misc_to_attr[ICON_GLOW]);
                                glowRect.size.width = graf_width;
                                glowRect.size.height = graf_height;
                                draw_image_tile(
                                    nsContext,
                                    ctx,
                                    pict_image,
                                    glowRect,
                                    destinationRect,
                                    NSCompositeSourceOver);
                            }
                            /*
                             * Skip drawing the foreground if it is the same
                             * as the background.
                             */
                            if (sourceRect.origin.x != terrainRect.origin.x ||
                                    sourceRect.origin.y != terrainRect.origin.y) {
                                draw_image_tile(
                                    nsContext,
                                    ctx,
                                    pict_image,
                                    sourceRect,
                                    destinationRect,
                                    NSCompositeSourceOver);
                            }
                            if (alert) {
                                NSRect alertRect;

                                alertRect.origin.x = graf_width *
                                    (0x7F & misc_to_char[ICON_ALERT]);
                                alertRect.origin.y = graf_height *
                                    (0x7F & misc_to_attr[ICON_ALERT]);
                                alertRect.size.width = graf_width;
                                alertRect.size.height = graf_height;
                                draw_image_tile(
                                    nsContext,
                                    ctx,
                                    pict_image,
                                    alertRect,
                                    destinationRect,
                                    NSCompositeSourceOver);
                            }
                        } else {
                            draw_image_tile(
                                nsContext,
                                ctx,
                                pict_image,
                                sourceRect,
                                destinationRect,
                                NSCompositeCopy);
                        }
                        jx += step;
                    }

                    [nsContext setCompositingOperation:op];

                     NSRect rect =
                         [angbandContext rectInImageForTileAtX:ix Y:iy];
                     rect.size.width =
                         angbandContext.tileSize.width * (jx - ix);
                     [angbandContext setNeedsDisplayInBaseRect:rect];
                 }
                 ix = jx;
                 break;

            case CELL_CHANGE_WIPE:
            case CELL_CHANGE_TEXT:
                /*
                 * For a wiped region, treat it as if it had text (the only
                 * loss if it was not is some extra work rendering
                 * neighboring unchanged text).
                 */
                 jx = ix + 1;
                 while (1) {
                     if (jx >= angbandContext.cols) {
                         break;
                     }
                     enum PendingCellChangeType ctype =
                         [angbandContext.changes getCellChangeType:jx row:iy];
                     if (ctype != CELL_CHANGE_TEXT
                             && ctype != CELL_CHANGE_WIPE) {
                         break;
                     }
                     ++jx;
                 }
                 {
                     int isclip = ix;
                     int ieclip = jx - 1;
                     int isrend = ix;
                     int ierend = jx - 1;
                     int set_color = 1;
                     int alast = 0;
                     NSRect r;
                     int k;

                     query_before_text(
                         angbandContext.changes,
                         iy,
                         angbandContext.nColPre,
                         &isclip,
                         &isrend);
                     query_after_text(
                         angbandContext.changes,
                         iy,
                         angbandContext.nColPost,
                         &ieclip,
                         &ierend);
                     ix = ierend + 1;

                     /* Save the state since the clipping will be modified. */
                     CGContextSaveGState(ctx);

                     /* Clear the area where rendering will be done. */
                     k = isrend;
                     while (k <= ierend) {
                         int k1 = k + 1;

                         alast = get_background_color_index(
                             [angbandContext.changes getCellTextChange:k row:iy].color);
                         while (k1 <= ierend && alast ==
                             get_background_color_index(
                                  [angbandContext.changes getCellTextChange:k1 row:iy].color)) {
                             ++k1;
                         }
                         if (alast == -1) {
                             [[NSColor blackColor] set];
                         } else {
                              set_color_for_index(alast);
                         }
                         r = [angbandContext rectInImageForTileAtX:k Y:iy];
                         r.size.width = angbandContext.tileSize.width *
                              (k1 - k);
                         NSRectFill(r);
                         k = k1;
                    }

                    /*
                     * Clear the current path so it does not affect clipping.
                     * Then set the clipping rectangle.  Using
                     * CGContextSetTextDrawingMode() to include clipping does
                     * not appear to be necessary on 10.14 and is actually
                     * detrimental:  when displaying more than one character,
                     * only the first is visible.
                     */
                    CGContextBeginPath(ctx);
                    r = [angbandContext rectInImageForTileAtX:isclip Y:iy];
                    r.size.width = angbandContext.tileSize.width *
                        (ieclip - isclip + 1);
                    CGContextClipToRect(ctx, r);

                    /* Render. */
                    k = isrend;
                    while (k <= ierend) {
                        NSRect rectToDraw;
                        int anew;

                        if ([angbandContext.changes getCellChangeType:k row:iy]
                                == CELL_CHANGE_WIPE) {
                            /* Skip over since no rendering is necessary. */
                            ++k;
                            continue;
                        }

                        struct PendingTextChange textChange =
                            [angbandContext.changes getCellTextChange:k
                                           row:iy];
                        anew = textChange.color % MAX_COLORS;
                        if (set_color || alast != anew) {
                            set_color = 0;
                            alast = anew;
                            set_color_for_index(anew);
                        }

                        rectToDraw =
                            [angbandContext rectInImageForTileAtX:k Y:iy];
                        [angbandContext drawWChar:textChange.glyph
                            inRect:rectToDraw context:ctx];
                        ++k;
                    }

                    /*
                     * Inform the context that the area in the clipping
                     * rectangle needs to be redisplayed.
                     */
                    [angbandContext setNeedsDisplayInBaseRect:r];

                    CGContextRestoreGState(ctx);
                }
                break;
            }
        }
    }

    if (angbandContext.changes.cursorColumn >= 0
            && angbandContext.changes.cursorRow >= 0) {
        NSRect rect = [angbandContext
            rectInImageForTileAtX:angbandContext.changes.cursorColumn
            Y:angbandContext.changes.cursorRow];

        rect.size.width *= angbandContext.changes.cursorWidth;
        rect.size.height *= angbandContext.changes.cursorHeight;
        [[NSColor blueColor] set];
        NSFrameRectWithWidth(rect, 1);
        /* Invalidate that rect */
        [angbandContext setNeedsDisplayInBaseRect:rect];
    }

    [angbandContext unlockFocus];
}


/**
 * Do a "special thing"
 */
static errr Term_xtra_cocoa(int n, int v)
{
    errr result = 0;
    @autoreleasepool {
        AngbandContext* angbandContext =
            (__bridge AngbandContext*) (Term->data);

        /* Analyze */
        switch (n) {
            /* Make a noise */
        case TERM_XTRA_NOISE:
            NSBeep();
            break;

            /* Process random events */
        case TERM_XTRA_BORED:
            /*
             * Show or hide cocoa windows based on the subwindow flags set by
             * the user.
             */
            AngbandUpdateWindowVisibility();
            /* Process an event */
            (void)check_events(CHECK_EVENTS_NO_WAIT);
            break;

            /* Process pending events */
        case TERM_XTRA_EVENT:
            /* Process an event */
            (void)check_events(v);
            break;

            /* Flush all pending events (if any) */
        case TERM_XTRA_FLUSH:
            /* Hack -- flush all events */
            while (check_events(CHECK_EVENTS_DRAIN)) /* loop */;

            break;

            /* Hack -- Change the "soft level" */
        case TERM_XTRA_LEVEL:
            /*
             * Here we could activate (if requested), but I don't think
             * Angband should be telling us our window order (the user
             * should decide that), so do nothing.
             */
            break;

            /* Clear the screen */
        case TERM_XTRA_CLEAR:
        {
            [angbandContext lockFocus];
            [[NSColor blackColor] set];
            NSRect imageRect = {NSZeroPoint, [angbandContext imageSize]};
            NSRectFillUsingOperation(imageRect, NSCompositeCopy);
            [angbandContext unlockFocus];
            [angbandContext setNeedsDisplay:YES];
            break;
        }

            /* React to changes */
        case TERM_XTRA_REACT:
            result = Term_xtra_cocoa_react();
            break;

            /* Delay (milliseconds) */
        case TERM_XTRA_DELAY:
            /* If needed */
            if (v > 0) {
                double seconds = v / 1000.;
                NSDate* date = [NSDate dateWithTimeIntervalSinceNow:seconds];
                do {
                    NSEvent* event;
                    do {
                        event = [NSApp nextEventMatchingMask:-1
                                       untilDate:date
                                       inMode:NSDefaultRunLoopMode
                                       dequeue:YES];
                        if (event) send_event(event);
                    } while (event);
                } while ([date timeIntervalSinceNow] >= 0);
            }
            break;

            /* Draw the pending changes. */
        case TERM_XTRA_FRESH:
            Term_xtra_cocoa_fresh(angbandContext);
            [angbandContext.changes clear];
            break;

        default:
            /* Oops */
            result = 1;
            break;
        }
    }

    return result;
}

static errr Term_curs_cocoa(int x, int y)
{
    AngbandContext *angbandContext = (__bridge AngbandContext*) (Term->data);

    [angbandContext.changes markCursor:x row:y];

    /* Success */
    return 0;
}

/**
 * Draw a cursor that's two tiles wide.
 */
static errr Term_bigcurs_cocoa(int x, int y)
{
    AngbandContext *angbandContext = (__bridge AngbandContext*) (Term->data);

    [angbandContext.changes markBigCursor:x row:y cellsWide:2 cellsHigh:1];

    /* Success */
    return 0;
}

/**
 * Low level graphics (Assumes valid input)
 *
 * Erase "n" characters starting at (x,y)
 */
static errr Term_wipe_cocoa(int x, int y, int n)
{
    AngbandContext *angbandContext = (__bridge AngbandContext*) (Term->data);

    [angbandContext.changes markWipeRange:x row:y n:n];

    /* Success */
    return 0;
}

static errr Term_pict_cocoa(int x, int y, int n, const byte_hack *ap, const char *cp, const byte_hack *tap, const char *tcp)
{
    /* Paranoia: Bail if graphics aren't enabled */
    if (! graphics_are_enabled()) return -1;

    AngbandContext* angbandContext = (__bridge AngbandContext*) (Term->data);
    int step = (use_bigtile) ? 2 : 1;

    /*
     * In bigtile mode, it is sufficient that the bounds for the modified
     * region only encompass the left cell for the region affected by the
     * tile and that only that cell has to have the details of the changes.
     */
    for (int i = x; i < x + n * step; i += step) {
        int a = *ap;
        char c = *cp;
        int ta = *tap;
        char tc = *tcp;

        ap += step;
        cp += step;
        tap += step;
        tcp += step;
        if (use_graphics && (a & 0x80) && (c & 0x80)) {
            [angbandContext.changes markTileChange:i row:y
                foregroundCol:((byte)c & 0x3F) % pict_cols
                foregroundRow:((byte)a & 0x3F) % pict_rows
                backgroundCol:((byte)tc & 0x3F) % pict_cols
                backgroundRow:((byte)ta & 0x3F) % pict_rows
                mask:((c & GRAPHICS_ALERT_MASK) ? PICT_MASK_ALERT : PICT_MASK_NONE)
                | ((a & GRAPHICS_GLOW_MASK) ? PICT_MASK_GLOW : PICT_MASK_NONE)];
        }
    }

    /* Success */
    return 0;
}

/**
 * Low level graphics.  Assumes valid input.
 *
 * Draw several ("n") chars, with an attr, at a given location.
 */
static errr Term_text_cocoa(int x, int y, int n, byte_hack a, const char *cp)
{
    AngbandContext* angbandContext = (__bridge AngbandContext*) (Term->data);

    for (int i = 0; i < n; ++i) {
        [angbandContext.changes markTextChange:i+x row:y glyph:cp[i] color:a];
    }

    /* Success */
    return 0;
}

#if 0
/** From the Linux mbstowcs(3) man page:
 *   If dest is NULL, n is ignored, and the conversion  proceeds  as  above,
 *   except  that  the converted wide characters are not written out to mem‐
 *   ory, and that no length limit exists.
 */
static size_t Term_mbcs_cocoa(wchar_t *dest, const char *src, int n)
{
    int i;
    int count = 0;

    /* Unicode code point to UTF-8
     *  0x0000-0x007f:   0xxxxxxx
     *  0x0080-0x07ff:   110xxxxx 10xxxxxx
     *  0x0800-0xffff:   1110xxxx 10xxxxxx 10xxxxxx
     * 0x10000-0x1fffff: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
     * Note that UTF-16 limits Unicode to 0x10ffff. This code is not
     * endian-agnostic.
     */
    for (i = 0; i < n || dest == NULL; i++) {
        if ((src[i] & 0x80) == 0) {
            if (dest != NULL) dest[count] = src[i];
            if (src[i] == 0) break;
        } else if ((src[i] & 0xe0) == 0xc0) {
            if (dest != NULL) dest[count] = 
                            (((unsigned char)src[i] & 0x1f) << 6)| 
                            ((unsigned char)src[i+1] & 0x3f);
            i++;
        } else if ((src[i] & 0xf0) == 0xe0) {
            if (dest != NULL) dest[count] = 
                            (((unsigned char)src[i] & 0x0f) << 12) | 
                            (((unsigned char)src[i+1] & 0x3f) << 6) |
                            ((unsigned char)src[i+2] & 0x3f);
            i += 2;
        } else if ((src[i] & 0xf8) == 0xf0) {
            if (dest != NULL) dest[count] = 
                            (((unsigned char)src[i] & 0x0f) << 18) | 
                            (((unsigned char)src[i+1] & 0x3f) << 12) |
                            (((unsigned char)src[i+2] & 0x3f) << 6) |
                            ((unsigned char)src[i+3] & 0x3f);
            i += 3;
        } else {
            /* Found an invalid multibyte sequence */
            return (size_t)-1;
        }
        count++;
    }
    return count;
}
#endif

/**
 * Determine if the big tile mode (each tile is 2 grids in width) should be
 * used.
 */
static BOOL bigtiles_are_appropriate(void)
{
    if (! use_graphics) {
        return NO;
    }
    AngbandContext *term0_context =
        (__bridge AngbandContext*) (angband_term[0]->data);
    CGFloat textw = term0_context.tileSize.width;
    CGFloat texth = term0_context.tileSize.height;
    CGFloat wratio = pict_cell_width / textw;
    CGFloat hratio = pict_cell_height / texth;

    return (wratio > 1.5 * hratio) ? YES : NO;
}


/**
 * Handle redrawing for a change to the tile set, tile scaling, or main window
 * font.  Returns YES if the redrawing was initiated.  Otherwise returns NO.
 */
static BOOL redraw_for_tiles_or_term0_font(void)
{
    /*
     * do_cmd_redraw() will always clear, but only provides something
     * to replace the erased content if a character has been generated.
     * Therefore, only call it if a character has been generated.
     */
    if (game_in_progress && character_dungeon) {
        do_cmd_redraw();
        wakeup_event_loop();
        return YES;
    }
    return NO;
}

/**
 * Post a nonsense event so that our event loop wakes up
 */
static void wakeup_event_loop(void)
{
    /* Big hack - send a nonsense event to make us update */
    NSEvent *event = [NSEvent otherEventWithType:NSApplicationDefined location:NSZeroPoint modifierFlags:0 timestamp:0 windowNumber:0 context:NULL subtype:AngbandEventWakeup data1:0 data2:0];
    [NSApp postEvent:event atStart:NO];
}


/**
 * Create and initialize window number "i"
 */
static term *term_data_link(int i)
{
    NSArray *terminalDefaults = [[NSUserDefaults standardUserDefaults] valueForKey: AngbandTerminalsDefaultsKey];
    NSInteger rows = 24;
    NSInteger columns = 80;

    if( i < (int)[terminalDefaults count] )
    {
        NSDictionary *term = [terminalDefaults objectAtIndex: i];
        rows = [[term valueForKey: AngbandTerminalRowsDefaultsKey] integerValue];
        columns = [[term valueForKey: AngbandTerminalColumnsDefaultsKey] integerValue];
    }

    /* Allocate */
    term *newterm = ZNEW(term);

    /* Initialize the term */
    term_init(newterm, columns, rows, 256 /* keypresses, for some reason? */);
    
    /* Differentiate between BS/^h, Tab/^i, etc. */
    ////newterm->complex_input = TRUE;

    /* Use a "software" cursor */
    newterm->soft_cursor = TRUE;

    /* Disable the per-row flush notifications since they are not used. */
    newterm->never_frosh = TRUE;

    /* Erase with "white space" */
    newterm->attr_blank = TERM_WHITE;
    newterm->char_blank = ' ';
    
    /* Prepare the init/nuke hooks */
    newterm->init_hook = Term_init_cocoa;
    newterm->nuke_hook = Term_nuke_cocoa;
    
    /* Prepare the function hooks */
    newterm->xtra_hook = Term_xtra_cocoa;
    newterm->wipe_hook = Term_wipe_cocoa;
    newterm->curs_hook = Term_curs_cocoa;
    newterm->bigcurs_hook = Term_bigcurs_cocoa;
    newterm->text_hook = Term_text_cocoa;
    newterm->pict_hook = Term_pict_cocoa;
    /* newterm->mbcs_hook = Term_mbcs_cocoa; */
    
    /* Global pointer */
    angband_term[i] = newterm;
    
    return newterm;
}

/**
 * Load preferences from preferences file for current host+current user+
 * current application.
 */
static void load_prefs()
{
    NSUserDefaults *defs = [NSUserDefaults angbandDefaults];
    
    /* Make some default defaults */
    NSMutableArray *defaultTerms = [[NSMutableArray alloc] init];

    /*
     * The following default rows/cols were determined experimentally by first
     * finding the ideal window/font size combinations. But because of awful
     * temporal coupling in Term_init_cocoa(), it's impossible to set up the
     * defaults there, so we do it this way.
     */
    for( NSUInteger i = 0; i < ANGBAND_TERM_MAX; i++ )
    {
		int columns, rows;
		BOOL visible = YES;

        ////half: changed this block of code that shows window sizes
		switch( i )
		{
			case WINDOW_MAIN:
				columns = 104;
				rows = 35;
				break;
			case WINDOW_INVEN:
				columns = 79;
				rows = 25;
				break;
			case WINDOW_EQUIP:
				columns = 79;
				rows = 16;
				break;
			case WINDOW_COMBAT_ROLLS:
				columns = 79;
				rows = 20;
				break;
			case WINDOW_MONSTER:
				columns = 122;
				rows = 10;
				break;
			default:
				columns = 80;
				rows = 24;
				visible = NO;
				break;
		}

		NSDictionary *standardTerm = [NSDictionary dictionaryWithObjectsAndKeys:
									  [NSNumber numberWithInt: rows], AngbandTerminalRowsDefaultsKey,
									  [NSNumber numberWithInt: columns], AngbandTerminalColumnsDefaultsKey,
									  [NSNumber numberWithBool: visible], AngbandTerminalVisibleDefaultsKey,
									  nil];
        [defaultTerms addObject: standardTerm];
    }

    NSDictionary *defaults = [[NSDictionary alloc] initWithObjectsAndKeys:
                              FallbackFontName, @"FontName-0",
                              [NSNumber numberWithFloat:FallbackFontSizeMain], @"FontSize-0",
                              [NSNumber numberWithInt:60], AngbandFrameRateDefaultsKey,
                              [NSNumber numberWithBool:YES], AngbandSoundDefaultsKey,
                              [NSNumber numberWithInt:GRAPHICS_NONE], AngbandGraphicsDefaultsKey,
                              defaultTerms, AngbandTerminalsDefaultsKey,
                              nil];
    [defs registerDefaults:defaults];

    /* preferred graphics mode */
    arg_graphics = [defs integerForKey:AngbandGraphicsDefaultsKey];

#if 0
    [AngbandSoundCatalog sharedSounds].enabled =
        [defs boolForKey:AngbandSoundDefaultsKey];
#endif

    /* fps */
    frames_per_second = [defs integerForKey:AngbandFrameRateDefaultsKey];
    
    /* font */
    [AngbandContext
        setDefaultFont:[NSFont fontWithName:[defs valueForKey:@"FontName-0"]
                               size:[defs floatForKey:@"FontSize-0"]]];
    if (! [AngbandContext defaultFont]) {
        [AngbandContext
            setDefaultFont:[NSFont fontWithName:FallbackFontName
            size:FallbackFontSizeMain]];
        if (! [AngbandContext defaultFont]) {
            [AngbandContext
                setDefaultFont:[NSFont systemFontOfSize:FallbackFontSizeMain]];
            if (! [AngbandContext defaultFont]) {
                [AngbandContext
                    setDefaultFont:[NSFont systemFontOfSize:0.0]];
            }
        }
    }
}

/**
 * Return the path for Angband's lib directory and bail if it isn't found. The
 * lib directory should be in the bundle's resources directory, since it's
 * copied when built.
 */
static NSString *get_lib_directory(void)
{
    NSString *bundleLibPath = [[[NSBundle mainBundle] resourcePath] stringByAppendingPathComponent: AngbandDirectoryNameLib];
    BOOL isDirectory = NO;
    BOOL libExists = [[NSFileManager defaultManager] fileExistsAtPath: bundleLibPath isDirectory: &isDirectory];

    if( !libExists || !isDirectory )
    {
        NSLog( @"Sil: can't find %@/ in bundle: isDirectory: %d libExists: %d", AngbandDirectoryNameLib, isDirectory, libExists );

        NSAlert *alert = [[NSAlert alloc] init];
        /*
         * Note that NSCriticalAlertStyle was deprecated in 10.10.  The
         * replacement is NSAlertStyleCritical.
         */
        alert.alertStyle = NSCriticalAlertStyle;
        alert.messageText = @"MissingResources";
        alert.informativeText = @"Sil was unable to find the 'lib' folder, which should be in Sil.app/Contents/Resources, so must quit. Please report a bug on the Angband forums.";
        [alert addButtonWithTitle:@"Quit"];
        [alert runModal];
        exit(0);
    }

    return bundleLibPath;
}

/**
 * Return the path for the directory where Angband should look for its standard
 * user file tree.
 */
#if 0
static NSString *get_doc_directory(void)
{
    NSString *documents = [NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES) lastObject];

#if defined(SAFE_DIRECTORY)
    NSString *versionedDirectory = [NSString stringWithFormat: @"%@-%s", AngbandDirectoryNameBase, VERSION_STRING];
    return [documents stringByAppendingPathComponent: versionedDirectory];
#else
    return [documents stringByAppendingPathComponent: AngbandDirectoryNameBase];
#endif
}
#endif

/**
 * Adjust directory paths as needed to correct for any differences needed by
 * Angband.  init_file_paths() currently requires that all paths provided have
 * a trailing slash and all other platforms honor this.
 *
 * \param originalPath The directory path to adjust.
 * \return A path suitable for Angband or nil if an error occurred.
 */
static NSString *AngbandCorrectedDirectoryPath(NSString *originalPath)
{
    if ([originalPath length] == 0) {
        return nil;
    }

    if (![originalPath hasSuffix: @"/"]) {
        return [originalPath stringByAppendingString: @"/"];
    }

    return originalPath;
}

#ifdef RUNTIME_PRIVATE_USER_PATH
extern const char *get_runtime_user_path(void)
{
    static int needs_init = 1;
    static char path[1024];

    if (needs_init) {
        @autoreleasepool {
            NSString *documents = [[NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES) lastObject]
                stringByAppendingPathComponent:@"/Sil"];

            [documents getFileSystemRepresentation:path maxLength:sizeof(path)];
        }
        needs_init = 0;
    }
    return path;
}
#endif

#ifdef PRIVATE_USER_PATH
/**
 * Create the directories for the user's files as needed.
 * Is the same as create_user_dir() in main.c.  Porting create_needed_dirs()
 * from Angband would avoid the code repetition.
 */
static void create_user_dir(void)
{
    char dirpath[1024];
    char subdirpath[1024];

    /* Get an absolute path from the filename */
#ifdef RUNTIME_PRIVATE_USER_PATH
    path_parse(dirpath, sizeof(dirpath), get_runtime_user_path());
#else
    path_parse(dirpath, sizeof(dirpath), PRIVATE_USER_PATH);
#endif

    /* Create the directory */
    mkdir(dirpath, 0700);

    /* Build the path to the variant-specific sub-directory */
    path_build(subdirpath, sizeof(subdirpath), dirpath, VERSION_NAME);

    /* Create the directory */
    mkdir(subdirpath, 0700);

#ifdef USE_PRIVATE_SAVE_PATH
    /* Build the path to the data sub-directory */
    path_build(dirpath, sizeof(dirpath), subdirpath, "data");

    /* Create the directory */
    mkdir(dirpath, 0700);

    /* Build the path toe the scores sub-directory */
    path_build(dirpath, sizeof(dirpath), subdirpath, "scores");

    /* Create the directory */
    mkdir(dirpath, 0700);

    /* Build the path to the savefile sub-directory */
    path_build(dirpath, sizeof(dirpath), subdirpath, "save");

    /* Create the directory */
    mkdir(dirpath, 0700);
#endif /* USE_PRIVATE_SAVE_PATH */
}
#endif

/**
 * Give Angband the base paths that should be used for the various directories
 * it needs. It will create any needed directories.
 */
static void prepare_paths_and_directories(void)
{
    char libpath[PATH_MAX + 1] = "\0";
    NSString *libDirectoryPath =
        AngbandCorrectedDirectoryPath(get_lib_directory());
    [libDirectoryPath getFileSystemRepresentation: libpath maxLength: sizeof(libpath)];

#if 0
    char basepath[PATH_MAX + 1] = "\0";
    NSString *angbandDocumentsPath =
        AngbandCorrectedDirectoryPath(get_doc_directory());
    [angbandDocumentsPath getFileSystemRepresentation: basepath maxLength: sizeof(basepath)];

    init_file_paths(libpath, libpath, basepath);
    create_needed_dirs();
#else
    init_file_paths(libpath);
#ifdef PRIVATE_USER_PATH
    create_user_dir();
#endif
#endif
}

#if 0
/**
 * Play sound effects asynchronously.  Select a sound from any available
 * for the required event, and bridge to Cocoa to play it.
 */
static void play_sound(int event)
{
    [[AngbandSoundCatalog sharedSounds] playSound:event];
}
#endif

/*
 * 
 */
static void init_windows(void)
{
    /* Create the main window */
    term *primary = term_data_link(0);
    
    /* Prepare to create any additional windows */
    int i;
    for (i=1; i < ANGBAND_TERM_MAX; i++) {
        term_data_link(i);
    }
    
    /* Activate the primary term */
    Term_activate(primary);
}


//// Sil-y: begin inserted block

static BOOL open_game(void)
{
    BOOL selectedSomething = NO;

    @autoreleasepool {
        char parsedPath[1024] = "";
        int panelResult;

        /* Get where we think the save files are */
        path_parse(parsedPath, sizeof(parsedPath), ANGBAND_DIR_SAVE);
        NSURL *startingDirectoryURL =
            [NSURL fileURLWithPath:[NSString stringWithCString:parsedPath
            encoding:NSASCIIStringEncoding] isDirectory:YES];

        /*
         * Get what we think the default save file name is. Default to the
         * empty string.
         */
        NSString *savefileName = [[NSUserDefaults angbandDefaults]
            stringForKey:@"SaveFile"];
        if (! savefileName) savefileName = @"";

        /* Set up an open panel */
        NSOpenPanel* panel = [NSOpenPanel openPanel];
        [panel setCanChooseFiles:YES];
        [panel setCanChooseDirectories:NO];
        [panel setResolvesAliases:YES];
        [panel setAllowsMultipleSelection:YES];
        [panel setTreatsFilePackagesAsDirectories:YES];
        [panel setDirectoryURL:startingDirectoryURL];
        [panel setNameFieldStringValue:savefileName];

        /* Run it */
        panelResult = [panel runModal];
        if (panelResult == NSOKButton) {
            NSArray* fileURLs = [panel URLs];
            if ([fileURLs count] > 0) {
                NSURL* savefileURL = (NSURL *)[fileURLs objectAtIndex:0];
                /*
                 * The path property doesn't do the right thing except for
                 * URLs with the file scheme.  getFileSystemRepresentation
                 * could be used, but that wan't introduced until OS 10.9.
                 */
                assert([[savefileURL scheme] isEqualToString:@"file"]);
                selectedSomething = [[savefileURL path]
                                        getCString:savefile
                                        maxLength:sizeof(savefile)
                                        encoding:NSMacOSRomanStringEncoding];
            }
        }

        if (selectedSomething) {
            /* Remember this so we can select it by default next time */
            record_current_savefile();

            /* Game is in progress */
            game_in_progress = YES;

            new_game = FALSE;
        }
    } 

    return selectedSomething;
}

/**
 * Open the tutorial savefile, stored in xtra/
 */
static void open_tutorial(void)
{
    @autoreleasepool {
        NSString* savefileName;

        /* Get the location of the tutorial savefile */
        savefileName = [[NSString stringWithCString:ANGBAND_DIR_XTRA
            encoding:NSASCIIStringEncoding]
            stringByAppendingPathComponent:@"/tutorial"];

        /* Put it in savefile */
        [savefileName getFileSystemRepresentation:savefile
            maxLength:sizeof(savefile)];

        /* Game is in progress */
        game_in_progress = YES;

        new_game = FALSE;
    }
}

//// Sil-y: end inserted block


/**
 * Handle quit_when_ready, by Peter Ammon,
 * slightly modified to check inkey_flag.
 */
static void quit_calmly(void)
{
    /* Quit immediately if game's not started */
    if (!game_in_progress || !character_generated) quit(NULL);

    /* Save the game and Quit (if it's safe) */
    if (inkey_flag)
    {
        /* Hack -- Forget messages */
        msg_flag = FALSE;

        /* Save the game */
        do_cmd_save_game();
        record_current_savefile();

        /* Quit */
        quit(NULL);
    }

    /* Wait until inkey_flag is set */
}



/**
 * Returns YES if we contain an AngbandView (and hence should direct our events
 * to Angband)
 */
static BOOL contains_angband_view(NSView *view)
{
    if ([view isKindOfClass:[AngbandView class]]) return YES;
    for (NSView *subview in [view subviews]) {
        if (contains_angband_view(subview)) return YES;
    }
    return NO;
}


/**
 * Queue mouse presses if they occur in the map section of the main window.
 */
static void AngbandHandleEventMouseDown( NSEvent *event )
{
	/* Sil doesn't use mouse events; Angband does.  Comment it out. */
#if 0
	AngbandContext *angbandContext = [[[event window] contentView] angbandContext];
	AngbandContext *mainAngbandContext =
            (__bridge AngbandContext*) (angband_term[0]->data);

	if ([[event window] isKeyWindow] &&
            mainAngbandContext.primaryWindow &&
            [[event window] windowNumber] ==
            [mainAngbandContext.primaryWindow windowNumber])
	{
		int cols, rows, x, y;
		Term_get_size(&cols, &rows);
		NSSize tileSize = angbandContext.tileSize;
		NSSize border = angbandContext.borderSize;
		NSPoint windowPoint = [event locationInWindow];

		/*
		 * Adjust for border; add border height because window origin
		 * is at bottom
		 */
		windowPoint = NSMakePoint( windowPoint.x - border.width, windowPoint.y + border.height );

		NSPoint p = [[[event window] contentView] convertPoint: windowPoint fromView: nil];
		x = floor( p.x / tileSize.width );
		y = floor( p.y / tileSize.height );

		BOOL displayingMapInterface = (inkey_flag) ? YES : NO;

		/* Sidebar plus border == thirteen characters; top row is reserved. */
		/* Coordinates run from (0,0) to (cols-1, rows-1). */
		BOOL mouseInMapSection = (x > 13 && x <= cols - 1 && y > 0  && y <= rows - 2);

		/*
		 * If we are displaying a menu, allow clicks anywhere within
		 * the terminal bounds; if we are displaying the main game
		 * interface, only allow clicks in the map section
		 */
		if ((!displayingMapInterface && x >= 0 && x < cols &&
			y >= 0 && y < rows) ||
			(displayingMapInterface && mouseInMapSection))
		{
			/*
			 * [event buttonNumber] will return 0 for left click,
			 * 1 for right click, but this is safer
			 */
			int button = ([event type] == NSLeftMouseDown) ? 1 : 2;

#ifdef KC_MOD_ALT
			NSUInteger eventModifiers = [event modifierFlags];
			byte angbandModifiers = 0;
			angbandModifiers |= (eventModifiers & NSShiftKeyMask) ? KC_MOD_SHIFT : 0;
			angbandModifiers |= (eventModifiers & NSControlKeyMask) ? KC_MOD_CONTROL : 0;
			angbandModifiers |= (eventModifiers & NSAlternateKeyMask) ? KC_MOD_ALT : 0;
			button |= (angbandModifiers & 0x0F) << 4; /* encode modifiers in the button number (see Term_mousepress()) */
#endif

			Term_mousepress(x, y, button);
		}
	}
#endif

	/* Pass click through to permit focus change, resize, etc. */
	[NSApp sendEvent:event];
}



/**
 * Encodes an NSEvent Angband-style, or forwards it along.  Returns YES if the
 * event was sent to Angband, NO if Cocoa (or nothing) handled it
 */
static BOOL send_event(NSEvent *event)
{

    /* If the receiving window is not an Angband window, then do nothing */
    if (! contains_angband_view([[event window] contentView]))
    {
        [NSApp sendEvent:event];
        return NO;
    }

    /* Analyze the event */
    switch ([event type])
    {
        case NSKeyDown:
        {
            /* Try performing a key equivalent */
            if ([[NSApp mainMenu] performKeyEquivalent:event]) break;
            
            unsigned modifiers = [event modifierFlags];
            
            /* Send all NSCommandKeyMasks through */
            if (modifiers & NSCommandKeyMask)
            {
                [NSApp sendEvent:event];
                break;
            }
            
//// myshkin            if (! [[event characters] length]) break;
            
            /* Extract some modifiers */
            int mc = !! (modifiers & NSControlKeyMask);
            int ms = !! (modifiers & NSShiftKeyMask);
            int mo = !! (modifiers & NSAlternateKeyMask);
            int mx = !! (modifiers & NSCommandKeyMask);
            int kp = !! (modifiers & NSNumericPadKeyMask);
            
            
#if 0 //// myshkin

            /* Get the Angband char corresponding to this unichar */
            unichar c = [[event characters] characterAtIndex:0];
            keycode_t ch;
            switch (c) {
                /* Note that NSNumericPadKeyMask is set if any of the arrow
                 * keys are pressed. We don't want KC_MOD_KEYPAD set for
                 * those. See #1662 for more details. */
                case NSUpArrowFunctionKey: ch = ARROW_UP; kp = 0; break;
                case NSDownArrowFunctionKey: ch = ARROW_DOWN; kp = 0; break;
                case NSLeftArrowFunctionKey: ch = ARROW_LEFT; kp = 0; break;
                case NSRightArrowFunctionKey: ch = ARROW_RIGHT; kp = 0; break;
                case NSF1FunctionKey: ch = KC_F1; break;
                case NSF2FunctionKey: ch = KC_F2; break;
                case NSF3FunctionKey: ch = KC_F3; break;
                case NSF4FunctionKey: ch = KC_F4; break;
                case NSF5FunctionKey: ch = KC_F5; break;
                case NSF6FunctionKey: ch = KC_F6; break;
                case NSF7FunctionKey: ch = KC_F7; break;
                case NSF8FunctionKey: ch = KC_F8; break;
                case NSF9FunctionKey: ch = KC_F9; break;
                case NSF10FunctionKey: ch = KC_F10; break;
                case NSF11FunctionKey: ch = KC_F11; break;
                case NSF12FunctionKey: ch = KC_F12; break;
                case NSF13FunctionKey: ch = KC_F13; break;
                case NSF14FunctionKey: ch = KC_F14; break;
                case NSF15FunctionKey: ch = KC_F15; break;
                case NSHelpFunctionKey: ch = KC_HELP; break;
                case NSHomeFunctionKey: ch = KC_HOME; break;
                case NSPageUpFunctionKey: ch = KC_PGUP; break;
                case NSPageDownFunctionKey: ch = KC_PGDOWN; break;
                case NSBeginFunctionKey: ch = KC_BEGIN; break;
                case NSEndFunctionKey: ch = KC_END; break;
                case NSInsertFunctionKey: ch = KC_INSERT; break;
                case NSDeleteFunctionKey: ch = KC_DELETE; break;
                case NSPauseFunctionKey: ch = KC_PAUSE; break;
                case NSBreakFunctionKey: ch = KC_BREAK; break;
                    
                default:
                    if (c <= 0x7F)
                        ch = (char)c;
                    else
                        ch = '\0';
                    break;
            }
            
            /* override special keys */
            switch([event keyCode]) {
                case kVK_Return: ch = KC_ENTER; break;
                case kVK_Escape: ch = ESCAPE; break;
                case kVK_Tab: ch = KC_TAB; break;
                case kVK_Delete: ch = KC_BACKSPACE; break;
                case kVK_ANSI_KeypadEnter: ch = KC_ENTER; kp = 1; break;
            }
            
            /* Hide the mouse pointer */
            [NSCursor setHiddenUntilMouseMoves:YES];
            
            /* Enqueue it */
            if (ch != '\0')
            {
                
                /* Enqueue the keypress */
#ifdef KC_MOD_ALT
                byte mods = 0;
                if (mo) mods |= KC_MOD_ALT;
                if (mx) mods |= KC_MOD_META;
                if (mc && MODS_INCLUDE_CONTROL(ch)) mods |= KC_MOD_CONTROL;
                if (ms && MODS_INCLUDE_SHIFT(ch)) mods |= KC_MOD_SHIFT;
                if (kp) mods |= KC_MOD_KEYPAD;
                Term_keypress(ch, mods);
#else
                Term_keypress(ch);
#endif
            }

#endif //// myshkin
                   
            //// begin myshkin block
            int kc = [event keyCode];
            
            /* Hide the mouse pointer */
            [NSCursor setHiddenUntilMouseMoves:YES];

            /* Some combining characters will have length 0, e.g.
               option-E produces a COMBINING ACUTE ACCENT event. */
            if ([[event characters] length])
            {
                /* Get the Angband char corresponding to this unichar */
                unichar c = [[event characters] characterAtIndex:0];
                    
                /* handle special keys */
                switch(kc)
                {
                    case kVK_Return: c = '\r'; break;
                    case kVK_Escape: c = ESCAPE; break;
                    case kVK_Delete: c = '\010'; break;

                    /* Strip the keypad flag for arrow keys. */
                    case kVK_LeftArrow: case kVK_RightArrow:
                    case kVK_DownArrow: case kVK_UpArrow:
                        kp = 0;
                        break;
                }
                    
                /* Normal keys with at most control and shift modifiers
                   Note that e.g. control-shift-f should get converted
                   to macro trigger, unlike control-f */
                if (c <= 0x7F && !mo && !mx && !kp && !(c <= 0x20 && ms))
                {
                    /* Enqueue the keypress */
                    Term_keypress(c);
                    break;
                }
            }

            /* Enqueue these keys via macro trigger;
               compare main-win.c:AngbandWndProc() */

            /* Begin the macro trigger */
            Term_keypress(31);
            
            /* Send modifiers (but not keypad flag) */
            if (mc) Term_keypress('C');
            if (ms) Term_keypress('S');
            if (mo) Term_keypress('O');
            if (mx) Term_keypress('X');
            
            /* Introduce the key code */
            Term_keypress('x');
            
            /* Encode the key code */
            Term_keypress(hexsym[kc/16]);
            Term_keypress(hexsym[kc%16]);
            
            /* End the macro trigger */
            Term_keypress(13);

            //// end myshkin block

            
            break;
        }
            
        case NSLeftMouseDown:
		case NSRightMouseDown:
			AngbandHandleEventMouseDown(event);
            break;

        case NSApplicationDefined:
        {
            if ([event subtype] == AngbandEventWakeup)
            {
                return YES;
            }
            break;
        }
            
        default:
            [NSApp sendEvent:event];
            return YES;
    }
    return YES;
}

/**
 * Check for Events, return YES if we process any
 */
static BOOL check_events(int wait)
{ 
    BOOL result = YES;

    @autoreleasepool {
        /* Handles the quit_when_ready flag */
        if (quit_when_ready) quit_calmly();

        NSDate* endDate;
        if (wait == CHECK_EVENTS_WAIT) endDate = [NSDate distantFuture];
        else endDate = [NSDate distantPast];
    
        NSEvent* event;
        for (;;) {
            if (quit_when_ready)
            {
                /* send escape events until we quit */
                Term_keypress(0x1B);
                result = NO;
                break;
            }
            else {
                event = [NSApp nextEventMatchingMask:-1 untilDate:endDate
                               inMode:NSDefaultRunLoopMode dequeue:YES];

#if 0
                static BOOL periodicStarted = NO;

                if (!periodicStarted) {
                    [NSEvent startPeriodicEventsAfterDelay: 0.0
                             withPeriod: 0.2];
                    periodicStarted = YES;
                }
		else if (FALSE && periodicStarted) {
                    [NSEvent stopPeriodicEvents];
                    periodicStarted = NO;
		}

                if (OPT(animate_flicker) && wait && periodicStarted &&
                    [event type] == NSPeriodic) {
                    idle_update();
                }
#endif

                if (! event) {
                    result = NO;
                    break;
                }
                if (send_event(event)) break;
            }
        }
    }

    return result;
}

/**
 * Hook to tell the user something important
 */
static void hook_plog(const char * str)
{
    if (str)
    {
        NSString *string = [NSString stringWithCString:str encoding:NSMacOSRomanStringEncoding];
        NSAlert *alert = [[NSAlert alloc] init];

        alert.messageText = @"Danger Will Robinson";
        alert.informativeText = string;
        [alert runModal];
    }
}


/**
 * Hook to tell the user something, and then quit
 */
static void hook_quit(const char * str)
{
    plog(str);
    exit(0);
}

/* Used by Angband but by Sil.  Comment them out. */
#if 0

/** Set HFS file type and creator codes on a path */
static void cocoa_file_open_hook(const char *path, file_type ftype)
{
    @autoreleasepool {
        NSString *pathString = [NSString stringWithUTF8String:path];
        if (pathString)
        {   
            u32b mac_type = 'TEXT';
            if (ftype == FTYPE_RAW)
                mac_type = 'DATA';
            else if (ftype == FTYPE_SAVE)
                mac_type = 'SAVE';

            NSDictionary *attrs =
                [NSDictionary dictionaryWithObjectsAndKeys:
                              [NSNumber numberWithUnsignedLong:mac_type],
                              NSFileHFSTypeCode,
                              [NSNumber numberWithUnsignedLong:SIL_CREATOR],
                              NSFileHFSCreatorCode,
                              nil];
            [[NSFileManager defaultManager]
                setAttributes:attrs ofItemAtPath:pathString error:NULL];
        }
    }
}


/** A platform-native file save dialogue box, e.g. for saving character dumps */
static bool cocoa_get_file(const char *suggested_name, char *path, size_t len)
{
    NSSavePanel *panel = [NSSavePanel savePanel];
    char parsedPath[1024] = "";

    path_parse(parsedPath, sizeof(parsedPath), ANGBAND_DIR_USER);
    NSURL *directoryURL = [NSURL URLWithString:[NSString stringWithCString:parsedPath encoding:NSASCIIStringEncoding]];
    NSString *filename = [NSString stringWithCString:suggested_name encoding:NSASCIIStringEncoding];

    [panel setDirectoryURL:directoryURL];
    [panel setNameFieldStringValue:filename];
    if ([panel runModal] == NSOKButton) {
        const char *p = [[[panel URL] path] UTF8String];
        my_strcpy(path, p, len);
        return TRUE;
    }

    return FALSE;
}

#endif

//// Sil-y: begin inserted block

u32b _fcreator;
u32b _ftype;

/**
 * (Carbon) [via path_to_spec]
 * Set creator and filetype of a file specified by POSIX-style pathname.
 * Returns 0 on success, -1 in case of errors.
 */
extern void fsetfileinfo(cptr pathname, u32b fcreator, u32b ftype)
{
    @autoreleasepool {
        NSString *pathString = [NSString stringWithUTF8String:pathname];
        if (pathString) {
            NSDictionary *attrs =
                [NSDictionary dictionaryWithObjectsAndKeys:
                [NSNumber numberWithUnsignedLong:ftype],
                NSFileHFSTypeCode,
                [NSNumber numberWithUnsignedLong:SIL_CREATOR],
                NSFileHFSCreatorCode,
                nil];
            [[NSFileManager defaultManager]
                setAttributes:attrs ofItemAtPath:pathString error:NULL];
        }
    }
}

//// Sil-y: end inserted block

/*** Main program ***/

@implementation AngbandAppDelegate

@synthesize commandMenu=_commandMenu;
@synthesize commandMenuTagMap=_commandMenuTagMap;

- (IBAction)newGame:sender
{
    /* Game is in progress */
    game_in_progress = YES;

    Term_keypress(ESCAPE); //// half: needed to break out of the text-based start menu
}

- (IBAction)editFont:sender
{
    NSFontPanel *panel = [NSFontPanel sharedFontPanel];
    NSFont *termFont = [AngbandContext defaultFont];

    int i;
    for (i=0; i < ANGBAND_TERM_MAX; i++) {
        AngbandContext *context =
            (__bridge AngbandContext*) (angband_term[i]->data);
        if ([context isMainWindow]) {
            termFont = [context angbandViewFont];
            break;
        }
    }

    [panel setPanelFont:termFont isMultiple:NO];
    [panel orderFront:self];
}

/**
 * Implement NSObject's changeFont() method to receive a notification about the
 * changed font.  Note that, as of 10.14, changeFont() is deprecated in
 * NSObject - it will be removed at some point and the application delegate
 * will have to be declared as implementing the NSFontChanging protocol.
 */
- (void)changeFont:(id)sender
{
    int mainTerm;
    for (mainTerm=0; mainTerm < ANGBAND_TERM_MAX; mainTerm++) {
        AngbandContext *context =
            (__bridge AngbandContext*) (angband_term[mainTerm]->data);
        if ([context isMainWindow]) {
            break;
        }
    }

    /* Bug #1709: Only change font for angband windows */
    if (mainTerm == ANGBAND_TERM_MAX) return;

    NSFont *oldFont = [AngbandContext defaultFont];
    NSFont *newFont = [sender convertFont:oldFont];
    if (! newFont) return; /*paranoia */

    /* Store as the default font if we changed the first term */
    if (mainTerm == 0) {
        [AngbandContext setDefaultFont:newFont];
    }

    /* Record it in the preferences */
    NSUserDefaults *defs = [NSUserDefaults angbandDefaults];
    [defs setValue:[newFont fontName] 
        forKey:[NSString stringWithFormat:@"FontName-%d", mainTerm]];
    [defs setFloat:[newFont pointSize]
        forKey:[NSString stringWithFormat:@"FontSize-%d", mainTerm]];

    NSDisableScreenUpdates();

    /* Update window */
    AngbandContext *angbandContext = (__bridge AngbandContext*)
        (angband_term[mainTerm]->data);
    [(id)angbandContext setSelectionFont:newFont adjustTerminal: YES];

    NSEnableScreenUpdates();

    if (mainTerm == 0 && use_graphics) {
        if (bigtiles_are_appropriate()) {
            if (! use_bigtile) {
                use_bigtile = TRUE;
                tile_multipliers_changed = 1;
            }
        } else {
            if (use_bigtile) {
                use_bigtile = FALSE;
                tile_multipliers_changed = 1;
            }
        }
    }

    if (mainTerm != 0 || ! redraw_for_tiles_or_term0_font()) {
        [(id)angbandContext requestRedraw];
    }
}

- (IBAction)openGame:sender
{
    if (open_game()) {
        Term_keypress(ESCAPE); //// half: needed to break out of the text-based start menu
    }
}

- (IBAction)saveGame:sender
{
    /* Hack -- Forget messages */
    msg_flag = FALSE;
    
    /* Save the game */
    do_cmd_save_game();
    
    /*
     * Record the current save file so we can select it by default next time.
     * It's a little sketchy that this only happens when we save through the
     * menu; ideally game-triggered saves would trigger it too.
     */
    record_current_savefile();
}

/* Entry point for initializing Angband */
- (void)beginGame
{
    @autoreleasepool {
        /* Used by Angband but not by Sil. */
#if 0
        /* Set the command hook */
        cmd_get_hook = textui_get_cmd;
#endif

        /* Hooks in some "z-util.c" hooks */
        plog_aux = hook_plog;
        quit_aux = hook_quit;

        /* Used by Angband but not by Sil. */
#if 0
        /* Hook in to the file_open routine */
        file_open_hook = cocoa_file_open_hook;

        /* Hook into file saving dialogue routine */
        get_file = cocoa_get_file;
#endif

        /* Initialize file paths */
        prepare_paths_and_directories();

        /* Note the "system" */
        ANGBAND_SYS = "mac";

        /* Load preferences */
        load_prefs();

        /* Prepare the windows */
        init_windows();

        /* Set up game event handlers */
        /* init_display(); */

        /* Register the sound hook */
        /* sound_hook = play_sound; */

        /* Initialize some save file stuff */
        player_egid = getegid();

        /* We are now initialized */
        initialized = YES;

        /* Handle pending events (most notably update) and flush input */
        Term_flush();

        /* Mark ourself as the file creator */
        _fcreator = SIL_CREATOR;

        /* Default to saving a "text" file */
        _ftype = 'TEXT';

	init_angband();

        use_background_colors = TRUE;
    }

    while (1) {
        @autoreleasepool {
            /* Let the player choose a savefile or start a new game */
            if (!game_in_progress) {
                int choice = 0;
                int highlight = 1;

                if (p_ptr->is_dead) highlight = 4;

                /* Process Events until "new" or "open" is selected */
                while (!game_in_progress) {
                    choice = initial_menu(&highlight);

                    switch (choice) {
                    case 1:
                        open_tutorial();
                        break;
                    case 2:
                        game_in_progress = YES;
                        new_game = TRUE;
                        break;
                    case 3:
                        open_game();
                        break;
                    case 4:
                        quit(NULL);
                        break;
                    }
		}
            }

            /* Handle pending events (most notably update) and flush input */
            Term_flush();

            /*
             * Play a game -- "new_game" is set by "new", "open" or the open
             * document event handler as appropriate
             */
            play_game(new_game);

            /* Rerun the first initialization routine */
            /* init_stuff(); */

            /* Do some more between-games initialization */
            re_init_some_things();

            /* Game no longer in progress */
            game_in_progress = NO;
        }
    }
}

/**
 * Implement NSObject's validateMenuItem() method to override enabling or
 * disabling a menu item.  Note that, as of 10.14, validateMenuItem() is
 * deprecated in NSObject - it will be removed at some point and the
 * application delegate will have to be declared as implementing the
 * NSMenuItemValidation protocol.
 */
- (BOOL)validateMenuItem:(NSMenuItem *)menuItem
{
    SEL sel = [menuItem action];
    NSInteger tag = [menuItem tag];

    if( tag >= AngbandWindowMenuItemTagBase && tag < AngbandWindowMenuItemTagBase + ANGBAND_TERM_MAX )
    {
        if( tag == AngbandWindowMenuItemTagBase )
        {
            /* The main window should always be available and visible */
            return YES;
        }
        else
        {
            /*
             * Another window is only usable after Term_init_cocoa() has
             * been called for it.  For Angband, if window_flag[i] is nonzero
             * then that has happened for window i.  For Sil, that is not the
             * case so also test angband_term[i]->data.
             */
            NSInteger subwindowNumber = tag - AngbandWindowMenuItemTagBase;
            return (angband_term[subwindowNumber]->data != 0
                    && op_ptr->window_flag[subwindowNumber] > 0);
        }

        return NO;
    }

    if (sel == @selector(newGame:))
    {
        return ! game_in_progress;
    }
    else if (sel == @selector(editFont:))
    {
        return YES;
    }
    else if (sel == @selector(openGame:))
    {
        return ! game_in_progress;
    }
    else if (sel == @selector(setRefreshRate:) &&
             [[menuItem parentItem] tag] == 150)
    {
        NSInteger fps = [[NSUserDefaults standardUserDefaults] integerForKey:AngbandFrameRateDefaultsKey];
        [menuItem setState: ([menuItem tag] == fps)];
        return YES;
    }
    else if( sel == @selector(setGraphicsMode:) )
    {
        NSInteger requestedGraphicsMode = [[NSUserDefaults standardUserDefaults] integerForKey:AngbandGraphicsDefaultsKey];
        [menuItem setState: (tag == requestedGraphicsMode)];
        /*
         * Only allow changes to the graphics mode when at the splash screen
         * or in the game proper and at a command prompt.  In other situations
         * the saved screens for overlayed menus could have tile references
         * that become outdated when the graphics mode is changed.
         */
        return (!game_in_progress || (character_generated && inkey_flag)) ?
            YES : NO;
    }
    else if( sel == @selector(sendAngbandCommand:) ||
		sel == @selector(saveGame:) )
    {
        /*
	 * we only want to be able to send commands during an active game
	 * after the birth screens when the core is waiting for a player
	 * command
	 */
        return !!game_in_progress && character_generated && inkey_flag;
    }
    else return YES;
}


- (IBAction)setRefreshRate:(NSMenuItem *)menuItem
{
    frames_per_second = [menuItem tag];
    [[NSUserDefaults angbandDefaults] setInteger:frames_per_second forKey:AngbandFrameRateDefaultsKey];
}

- (void)setGraphicsMode:(NSMenuItem *)sender
{
    /* We stashed the graphics mode ID in the menu item's tag */
    arg_graphics = [sender tag];

    /* Stash it in UserDefaults */
    [[NSUserDefaults angbandDefaults] setInteger:arg_graphics forKey:AngbandGraphicsDefaultsKey];

    redraw_for_tiles_or_term0_font();
}

- (void)selectWindow: (id)sender
{
    NSInteger subwindowNumber = [(NSMenuItem *)sender tag] - AngbandWindowMenuItemTagBase;
    AngbandContext *context = (__bridge AngbandContext*)
        (angband_term[subwindowNumber]->data);
    [context.primaryWindow makeKeyAndOrderFront: self];
    [context saveWindowVisibleToDefaults: YES];
}

- (void)prepareWindowsMenu
{
    /*
     * Get the window menu with default items and add a separator and
     * item for the main window.
     */
    NSMenu *windowsMenu = [[NSApplication sharedApplication] windowsMenu];
    [windowsMenu addItem: [NSMenuItem separatorItem]];

    NSMenuItem *angbandItem = [[NSMenuItem alloc] initWithTitle: @"Sil" action: @selector(selectWindow:) keyEquivalent: @"0"];
    [angbandItem setTarget: self];
    [angbandItem setTag: AngbandWindowMenuItemTagBase];
    [windowsMenu addItem: angbandItem];

    /* Add items for the additional term windows */
    for( NSInteger i = 1; i < ANGBAND_TERM_MAX; i++ )
    {
////half        NSString *title = [NSString stringWithFormat: @"Term %ld", (long)i];
        NSString *title = [NSString stringWithUTF8String: angband_term_name[i]]; ////half
        NSMenuItem *windowItem = [[NSMenuItem alloc] initWithTitle: title action: @selector(selectWindow:) keyEquivalent: @""];
        [windowItem setTarget: self];
        [windowItem setTag: AngbandWindowMenuItemTagBase + i];
        [windowsMenu addItem: windowItem];
    }
}

/**
 * Send a command to Angband via a menu item. This places the appropriate key
 * down events into the queue so that it seems like the user pressed them
 * (instead of trying to use the term directly).
 */
- (void)sendAngbandCommand: (id)sender
{
    NSMenuItem *menuItem = (NSMenuItem *)sender;
    NSString *command = [self.commandMenuTagMap objectForKey: [NSNumber numberWithInteger: [menuItem tag]]];
    AngbandContext* context =
        (__bridge AngbandContext*) (angband_term[0]->data);
    NSInteger windowNumber = [context.primaryWindow windowNumber];

    /* Send a \ to bypass keymaps */
    NSEvent *escape = [NSEvent keyEventWithType: NSKeyDown
                                       location: NSZeroPoint
                                  modifierFlags: 0
                                      timestamp: 0.0
                                   windowNumber: windowNumber
                                        context: nil
                                     characters: @"\\"
                    charactersIgnoringModifiers: @"\\"
                                      isARepeat: NO
                                        keyCode: 0];
    [[NSApplication sharedApplication] postEvent: escape atStart: NO];

    /* Send the actual command (from the original command set) */
    NSEvent *keyDown = [NSEvent keyEventWithType: NSKeyDown
                                        location: NSZeroPoint
                                   modifierFlags: 0
                                       timestamp: 0.0
                                    windowNumber: windowNumber
                                         context: nil
                                      characters: command
                     charactersIgnoringModifiers: command
                                       isARepeat: NO
                                         keyCode: 0];
    [[NSApplication sharedApplication] postEvent: keyDown atStart: NO];
}

/**
 *  Set up the command menu dynamically, based on CommandMenu.plist.
 */
- (void)prepareCommandMenu
{
    NSString *commandMenuPath = [[NSBundle mainBundle] pathForResource: @"CommandMenu" ofType: @"plist"];
    NSArray *commandMenuItems = [[NSArray alloc] initWithContentsOfFile: commandMenuPath];
    NSMutableDictionary *angbandCommands = [[NSMutableDictionary alloc] init];
    NSInteger tagOffset = 0;

    for( NSDictionary *item in commandMenuItems )
    {
        BOOL useShiftModifier = [[item valueForKey: @"ShiftModifier"] boolValue];
        BOOL useOptionModifier = [[item valueForKey: @"OptionModifier"] boolValue];
        NSUInteger keyModifiers = NSCommandKeyMask;
        keyModifiers |= (useShiftModifier) ? NSShiftKeyMask : 0;
        keyModifiers |= (useOptionModifier) ? NSAlternateKeyMask : 0;

        NSString *title = [item valueForKey: @"Title"];
        NSString *key = [item valueForKey: @"KeyEquivalent"];
        NSMenuItem *menuItem = [[NSMenuItem alloc] initWithTitle: title action: @selector(sendAngbandCommand:) keyEquivalent: key];
        [menuItem setTarget: self];
        [menuItem setKeyEquivalentModifierMask: keyModifiers];
        [menuItem setTag: AngbandCommandMenuItemTagBase + tagOffset];
        [self.commandMenu addItem: menuItem];

        NSString *angbandCommand = [item valueForKey: @"AngbandCommand"];
        [angbandCommands setObject: angbandCommand forKey: [NSNumber numberWithInteger: [menuItem tag]]];
        tagOffset++;
    }

    self.commandMenuTagMap = [[NSDictionary alloc]
                                 initWithDictionary: angbandCommands];
}

- (void)awakeFromNib
{
    [super awakeFromNib];

    [self prepareWindowsMenu];
    [self prepareCommandMenu];
}

- (void)applicationDidFinishLaunching:sender
{
    [self beginGame];
    
    /*
     * Once beginGame finished, the game is over - that's how Angband works,
     * and we should quit
     */
    game_is_finished = YES;
    [NSApp terminate:self];
}

- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)sender
{
    if (!p_ptr->playing || game_is_finished)
    {
        quit_when_ready = true;
        return NSTerminateNow;
    }
    else if (! inkey_flag)
    {
        /* For compatibility with other ports, do not quit in this case */
        return NSTerminateCancel;
    }
    else
    {
        /*
         * Post an escape event so that we can return from our get-key-event
         * function
         */
        wakeup_event_loop();
        quit_when_ready = true;
        /*
         * Must return Cancel, not Later, because we need to get out of the
         * run loop and back to Angband's loop
         */
        return NSTerminateCancel;
    }
}

/**
 * Dynamically build the Graphics menu
 */
- (void)menuNeedsUpdate:(NSMenu *)menu {
    
    /* Only the graphics menu is dynamic */
    if (! [[menu title] isEqualToString:@"Graphics"])
        return;
    
    /*
     * If it's non-empty, then we've already built it. Currently graphics modes
     * won't change once created; if they ever can we can remove this check.
     * Note that the check mark does change, but that's handled in
     * validateMenuItem: instead of menuNeedsUpdate:
     */
    if ([menu numberOfItems] > 0)
        return;
    
    /* This is the action for all these menu items */
    SEL action = @selector(setGraphicsMode:);
    
    /* Add an initial Classic ASCII menu item */
    NSMenuItem *item = [menu addItemWithTitle:@"Classic ASCII" action:action keyEquivalent:@""];
    [item setTag:GRAPHICS_NONE];

    item = [menu addItemWithTitle:@"MicroChasm's Tiles" action:action keyEquivalent:@""];
    [item setTag:GRAPHICS_MICROCHASM];
}

/**
 * Delegate method that gets called if we're asked to open a file.
 */
- (void)application:(NSApplication *)sender openFiles:(NSArray *)filenames
{
    /* Can't open a file once we've started */
    if (game_in_progress) {
        [[NSApplication sharedApplication]
            replyToOpenOrPrint:NSApplicationDelegateReplyFailure];
        return;
    }

    /* We can only open one file. Use the last one. */
    NSString *file = [filenames lastObject];
    if (! file) {
        [[NSApplication sharedApplication]
            replyToOpenOrPrint:NSApplicationDelegateReplyFailure];
        return;
    }

    /* Put it in savefile */
    if (! [file getFileSystemRepresentation:savefile maxLength:sizeof savefile]) {
        [[NSApplication sharedApplication]
            replyToOpenOrPrint:NSApplicationDelegateReplyFailure];
        return;
    }

    game_in_progress = YES;
    new_game = FALSE;

    /*
     * Wake us up in case this arrives while we're sitting at the Welcome
     * screen!
     */
    wakeup_event_loop();
    if (initialized) {
        Term_keypress(ESCAPE);
    }

    [[NSApplication sharedApplication]
        replyToOpenOrPrint:NSApplicationDelegateReplySuccess];
}

@end

int main(int argc, char* argv[])
{
    NSApplicationMain(argc, (void*)argv);    
    return (0);
}

#endif /* MACINTOSH || MACH_O_CARBON */
