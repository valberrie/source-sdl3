# Source Engine: Community Edition (SDL3 port)

## overview

this is a port of the 2018 leak of the Source Engine to use SDL3+. since SDL3 just recently hit ABI lock i figured this would be a good little project, especially considering i was just recently working with Source

## status

has only been tested as of now on linux, for a linux target, with hl2 as the target game [although that shouldn't matter at all]. 

## issues

 - fullscreen and changing resolutions is currently broken.
 - audio is working as intended, however at this time i don't know if audio *input* will work \ this will have to be tested with a multiplayer game.
 - the input system, including gamepads, is largely working. one issue is text input; due to a change to an SDL function call, this was done with a sort of hacky solution where you have to be "keyboard focused" on the application. in practice this means that you'll likely have to click into the application window to type for the first time. not the *most* annoying but i don't like it.
 - no other known issues at this time, although i'm sure there will be more lmao

## EOF

more info about SE:CE in 'Source\_Engine\_CE.md' 
