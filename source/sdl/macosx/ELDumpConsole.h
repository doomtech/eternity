//
//  ELDumpConsole.h
//  EternityLaunch
//
//  Created by Ioan on 19.06.2012.
/*
 Copyright (C) 2012  Ioan Chera
 
 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 
 */

#import <Cocoa/Cocoa.h>


@interface ELDumpConsole : NSWindowController {
	IBOutlet NSTextView *textView;
	NSMutableString *log;
	NSAttributedString *attrStr;
	NSFileHandle *outHandle;
}

@property (assign) NSMutableString *log;
@property (assign) NSTextView *textView;

-(id)initWithWindowNibName:(NSString *)windowNibName;
-(void)dealloc;
-(void)startLogging;
-(void)dataReady:(NSNotification *)notification;
-(void)showInstantLog;

@end
