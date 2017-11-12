# Code designed and written by: Muhammad Ahmed Saqib
# Andrew ID: msaqib
# File Created: November 11, 11:00am
# Modification History:
# Start End
# 11/11 11:00am 12/11 12:30am
# 12/11 8:45am 12/11 1:10 pm



import pygame
from Tkinter import *
from PIL import ImageTk, Image
def mainFunction():
    pygame.quit()
    white =(255,255,255)
    black = (0,0,0)
    green = (0,255,0)
    red = (255,0,0)
    blue = (0,0,255)
    gameScreen = pygame.display.set_mode((800, 800))
    pygame.display.set_caption("Checkers")
    gameEnd = False
    clock = pygame.time.Clock()
    drawBoard(gameScreen)
    pieceList = drawPieces(gameScreen)
    locationList = boxLocations(gameScreen)
    loc = False
    global Turn
    Turn = True
    variable = False
    BN = -1
    while gameEnd == False:
        for event in pygame.event.get():
                if event.type == pygame.QUIT:
                        gameEnd = True
                if event.type == pygame.MOUSEBUTTONDOWN:
                    mPosition = pygame.mouse.get_pos()
                    loc = False
                    i = 1
                    if variable == False:
                       variable = evaluateMove(gameScreen,locationList,pieceList,variable,mPosition,BN)
                    
                    if variable == "H":
                        while i < len(locationList):
                            if gameScreen.get_at((locationList[i][0]+46,locationList[i][1]))  == (0,0,255,255):
                                if gameScreen.get_at((locationList[i-2][0]+46,locationList[i-2][1])) == (255,255,255,255) and locationList[i-1] % 8 != 0 :
                                   pygame.draw.rect(gameScreen,green,pygame.Rect((locationList[i][0] - 50),(locationList[i][1] - 50),100,100))
                                else:
                                   pygame.draw.rect(gameScreen,white,pygame.Rect((locationList[i][0] - 50),(locationList[i][1] - 50),100,100))
                            i += 2
                        pygame.display.flip()                        
                        BN = highlightMoves(gameScreen,pieceList,locationList,loc,mPosition)
                        variable = False
                    elif variable == "N" or variable == "K":
                        variable = False
                    elif variable == "M":
                        variable = False
        clock.tick(50)          
        pygame.display.flip()
    pygame.quit()


def drawBoard(gameScreen):
    white =(255,255,255)
    black = (0,0,0)
    green = (0,255,0)
    red = (255,0,0)
    x = 0
    y = 0
    for e in range(8):
        for i in range(8):
          if e%2 == 0:
              if i%2 == 0:
                pygame.draw.rect(gameScreen,green,pygame.Rect(x,y,100,100))
              else:
                pygame.draw.rect(gameScreen,white,pygame.Rect(x,y,100,100))
          else:
              if i%2 == 0:
                pygame.draw.rect(gameScreen,white,pygame.Rect(x,y,100,100))
              else:
                pygame.draw.rect(gameScreen,green,pygame.Rect(x,y,100,100))
          x += 100
        y += 100
        x = 0

def drawPieces(gameScreen):
    white =(255,255,255)
    black = (0,0,0)
    green = (0,255,0)
    red = (255,0,0)
    x = 50
    y = 50
    a = 150                
    redPiece = -1
    greenPiece = 11
    pieceList = []
    for e in range(8):
        for i in range(4):
            if e == 0 or e == 1 or e == 2:
                 if e%2 == 0:
                        pygame.draw.circle(gameScreen,red,(a,y),40)
                        redPiece += 1
                        pieceList = pieceList + [redPiece]
                        pieceList = pieceList + [(a,y)]
                        a += 200
                 else:
                        pygame.draw.circle(gameScreen,red,(x,y),40)
                        redPiece += 1
                        pieceList = pieceList + [redPiece]
                        pieceList = pieceList + [(x,y)]
                        x += 200
            if e == 5 or e == 6 or e == 7:
                if e%2 == 0:
                        pygame.draw.circle(gameScreen,green,(a,y),40)
                        greenPiece += 1
                        pieceList = pieceList + [greenPiece]
                        pieceList = pieceList + [(a,y)]
                        a += 200
                else:
                        pygame.draw.circle(gameScreen,green,(x,y),40)
                        greenPiece += 1
                        pieceList = pieceList + [greenPiece]
                        pieceList = pieceList + [(x,y)]
                        x += 200         
        a = 150        
        x = 50
        y += 100
    return pieceList

def boxLocations(gameScreen):
    locationList = []
    x = 50
    y = 50
    Box = 0
    for e in range(8):
        for i in range(8):
            locationList = locationList + [Box]
            locationList = locationList + [(x,y)]
            x += 100
            Box += 1
        y += 100
        x=50
    return locationList
#not for moves where the tile is occupied
def highlightMoves(gameScreen,pieceList,locationList,loc,mPosition):
     global Turn
     white =(255,255,255)
     black = (0,0,0)
     green = (0,255,0)
     red = (255,0,0)
     blue = (0,0,255)
     Location1 = ' '
     Location2 = ' '
     i = 1
     while i < len(locationList) and loc == False:
        if i%2 != 0:
            if mPosition[0] < (locationList[i][0] + 50) and mPosition[0] > (locationList[i][0] - 50) and mPosition[1] < (locationList[i][1] + 50) and abs(locationList[i][1] - mPosition[1]) < 50:
                BN = locationList[i-1]
                for d in range(len(pieceList)):
                    if locationList[i] == pieceList[d]:
                        
                          
                          if locationList[i-1]%8 != 7 and locationList[i-1]%8 != 0:
                              if pieceList[d-1] < 12 and Turn == True:
                                Location1 = locationList[i-1] + 7
                                Location2 = locationList[i-1] + 9
                               
                              else:
                                if pieceList[d-1] >= 12 and Turn == False:
                                  Location1 = locationList[i-1] - 7
                                  Location2 = locationList[i-1] - 9
                                  print Location1, Location2
                              for m in range(len(locationList)):
                                  if locationList[m] == Location1:
                                      if pieceList[d-1] < 12 and Turn == True:                                         
                                         if locationList[m+1] not in pieceList:
                                           pygame.draw.rect(gameScreen,blue,pygame.Rect((locationList[m+1][0]-50),(locationList[m+1][1]-50),100,100))
                                           loc = True
                                      else:
                                        if pieceList[d-1] >= 12 and Turn == False:
                                         if locationList[m+1] not in pieceList:
                                           pygame.draw.rect(gameScreen,blue,pygame.Rect((locationList[m+1][0]-50),(locationList[m+1][1]-50),100,100)) 
                                           loc = True
                                      pygame.display.flip()
                                  if locationList[m] == Location2:                                   
                                      if pieceList[d-1] < 12 and Turn == True:
                                        if locationList[m+1] not in pieceList:  
                                         pygame.draw.rect(gameScreen,blue,pygame.Rect((locationList[m+1][0]-50),(locationList[m+1][1]-50),100,100))
                                      else: 
                                        if pieceList[d-1] >= 12 and Turn == False: 
                                         if locationList[m+1] not in pieceList:   
                                          pygame.draw.rect(gameScreen,blue,pygame.Rect((locationList[m+1][0]-50),(locationList[m+1][1]-50),100,100))
                                      
                                      pygame.display.flip()
                                      loc = True    
                          elif locationList[i-1]%8 == 7:
                               if pieceList[d-1] < 12 and Turn == True:
                                  Location1 = locationList[i-1] + 7
                               else:
                                 if pieceList[d-1] >= 12 and Turn == False:  
                                  Location1 = locationList[i-1] - 9 
                               for m in range(len(locationList)):
                                  if locationList[m] == Location1:
                                      if pieceList[d-1] < 12 and Turn == True:
                                        if locationList[m+1] not in pieceList:
                                         pygame.draw.rect(gameScreen,blue,pygame.Rect((locationList[m+1][0]-50),(locationList[m+1][1]-50),100,100))
                                      else:
                                       if pieceList[d-1] >= 12 and Turn == False:   
                                        if locationList[m+1] not in pieceList:
                                         pygame.draw.rect(gameScreen,blue,pygame.Rect((locationList[m+1][0]-50),(locationList[m+1][1]-50),100,100))
                                      loc = True
                                      pygame.display.flip()
                          elif locationList[i-1]%8 == 0:
                               if pieceList[d-1] < 12 and Turn == True:
                                 Location1 = locationList[i-1] + 9
                               else:
                                  if pieceList[d-1] >= 12 and Turn == False: 
                                   Location1 = locationList[i-1] - 7
                               for m in range(len(locationList)):
                                   if locationList[m] == Location1: 
                                      if pieceList[d-1] < 12 and Turn == True:
                                        if locationList[m+1] not in pieceList:
                                         pygame.draw.rect(gameScreen,blue,pygame.Rect((locationList[m+1][0]-50),(locationList[m+1][1]-50),100,100))
                                      else:
                                       if pieceList[d-1] >= 12 and Turn == False:   
                                        if locationList[m+1] not in pieceList:
                                         pygame.draw.rect(gameScreen,blue,pygame.Rect((locationList[m+1][0]-50),(locationList[m+1][1]-50),100,100))
                                      loc = True
                                      pygame.display.flip()                 
        i += 2            
     return BN   

def evaluateMove(gameScreen,locationList,pieceList,variable,mPosition,BN):
    i = 1
    a=1
    Bool1 = False
    global Turn
    while variable == False:
        if  gameScreen.get_at((mPosition[0],mPosition[1]))  == (0,0,255,255):
          while a < len(locationList) and Bool1 == False:   
            if mPosition[0] < (locationList[a][0] + 50) and mPosition[0] > (locationList[a][0] - 50) and mPosition[1] < (locationList[a][1] + 50) and abs(locationList[a][1] - mPosition[1]) < 50:
                boxNumber2 = locationList[a-1]
                if abs(BN - boxNumber2) == 7 or abs(BN - boxNumber2) == 9:
                    if Turn == True:
                        c = 1    
                        while c < len(locationList):
                            if gameScreen.get_at((locationList[c][0]+46,locationList[c][1]))  == (0,0,255,255):
                                if gameScreen.get_at((locationList[c-2][0]+46,locationList[c-2][1])) == (255,255,255,255) and locationList[c-1] % 8 != 0 :
                                   pygame.draw.rect(gameScreen,green,pygame.Rect((locationList[c][0] - 50),(locationList[c][1] - 50),100,100))
                                else:
                                   pygame.draw.rect(gameScreen,white,pygame.Rect((locationList[c][0] - 50),(locationList[c][1] - 50),100,100))
                            c += 2
                        pygame.draw.circle(gameScreen,red,(locationList[a][0],locationList[a][1]),40)
                        for b in range(len(locationList)):
                            if locationList[b] == BN:
                                Coords = locationList[b+1]
                        pygame.draw.rect(gameScreen,white,pygame.Rect((Coords[0]-50),(Coords[1]-50),100,100))
                        for v in range(len(pieceList)):
                            if (Coords[0],Coords[1]) == pieceList[v]:
                                pieceList[v] = (locationList[a][0],locationList[a][1])
                        if Turn == True:
                            Turn = False
                        else:
                            Turn = True
                        
                       
                    else:
                        c = 1    
                        while c < len(locationList):
                            if gameScreen.get_at((locationList[c][0]+46,locationList[c][1]))  == (0,0,255,255):
                                if gameScreen.get_at((locationList[c-2][0]+46,locationList[c-2][1])) == (255,255,255,255) and locationList[c-1] % 8 != 0 :
                                   pygame.draw.rect(gameScreen,green,pygame.Rect((locationList[c][0] - 50),(locationList[c][1] - 50),100,100))
                                else:
                                   pygame.draw.rect(gameScreen,white,pygame.Rect((locationList[c][0] - 50),(locationList[c][1] - 50),100,100))
                            c += 2
                        pygame.draw.circle(gameScreen,green,(locationList[a][0],locationList[a][1]),40)
                        for b in range(len(locationList)):
                            if locationList[b] == BN:
                                Coords = locationList[b+1]
                        for v in range(len(pieceList)):
                            if (Coords[0],Coords[1]) == pieceList[v]:
                                pieceList[v] = (locationList[a][0],locationList[a][1])
                        pygame.draw.rect(gameScreen,white,pygame.Rect((Coords[0]-50),(Coords[1]-50),100,100))
                        if Turn == True:
                            Turn = False
                        else:
                            Turn = True
                       
                    pygame.display.flip()
                Bool1=True
            a += 2
          Bool1 = False
          variable = 'M'
          return variable

        
        while i < len(locationList) and variable == False:
            if mPosition[0] < (locationList[i][0] + 50) and mPosition[0] > (locationList[i][0] - 50) and mPosition[1] < (locationList[i][1] + 50) and abs(locationList[i][1] - mPosition[1]) < 50:
                boxNumber2 = locationList[i-1]
                print BN
                print boxNumber2
                if gameScreen.get_at((locationList[i][0],locationList[i][1])) == gameScreen.get_at((locationList[i][0]+46,locationList[i][1])):
                    if Turn == True:
                        print "Here"
                        if abs(BN - boxNumber2) == 14:
                            print "here"
                            boxNumber3 = boxNumber2 - 7
                            Bool2 = False
                            n = 0
                            while n < len(locationList) and Bool2 == False:
                                for b in range(len(locationList)):
                                            if locationList[b] == BN:
                                              Coords = locationList[b+1]
                                if boxNumber3 == locationList[n]:
                                    Coord = locationList[n+1]
                                if boxNumber2 == locationList[n]:
                                    if locationList[n+1] not in pieceList:
                                        pygame.draw.circle(gameScreen,red,(locationList[n+1][0],locationList[n+1][1]),40)
                                        pygame.draw.rect(gameScreen,white,pygame.Rect((locationList[b][0]-50),(locationList[b][1]-50),100,100))
                                        pygame.draw.rect(gameScreen,white,pygame.Rect((Coord[0]-50),(Coord[1]-50),100,100))
                                        pygame.draw.rect(gameScreen,white,pygame.Rect((Coords[0]-50),(Coords[1]-50),100,100))
                                        r = 1
                                        while r < len(locationList):
                                            if gameScreen.get_at((locationList[r][0]+46,locationList[r][1]))  == (0,0,255,255):
                                                if gameScreen.get_at((locationList[r-2][0]+46,locationList[r-2][1])) == (255,255,255,255) and locationList[r-1] % 8 != 0 :
                                                   pygame.draw.rect(gameScreen,green,pygame.Rect((locationList[r][0] - 50),(locationList[r][1] - 50),100,100))
                                                else:
                                                   pygame.draw.rect(gameScreen,white,pygame.Rect((locationList[r][0] - 50),(locationList[r][1] - 50),100,100))
                                            r += 2
                                        pygame.display.flip()
                                        for v in range(len(pieceList)):
                                            if (Coords[0],Coords[1]) == pieceList[v]:
                                                pieceList[v] = (locationList[n+1][0],locationList[n+1][1])
                                        Turn = True
                                        variable = "K"
                                        return variable
                                        
                                    else:
                                        variable = "N"
                                        return variable
                                n += 2    
                if gameScreen.get_at((locationList[i][0],locationList[i][1])) == (255,0,0,255) and gameScreen.get_at((locationList[i][0]+46,locationList[i][1]))== (255,255,255,255):
                    if Turn == True:
                        variable = "H"
                        return variable
                    elif Turn == False:
                        variable = "N"  
                        return variable
                if gameScreen.get_at((locationList[i][0],locationList[i][1])) == (0,255,0,255) and gameScreen.get_at((locationList[i][0]+46,locationList[i][1]))== (255,255,255,255):
                    print"Here"
                    if Turn == False:
                       variable = "H"
                       return variable
                    elif Turn == True:
                       variable = "N"
                       return variable
            i += 2
                
               

pygame.init()
white =(255,255,255)
black = (0,0,0)
green = (0,255,0)
red = (255,0,0)
menuScreen = pygame.display.set_mode((1024,576))
pygame.display.set_caption("Menu for checkers")
gameRun = False
Menu = pygame.image.load('Menu1.jpg')
menuScreen.blit(Menu,(0,0))
pygame.draw.rect(menuScreen,white,pygame.Rect(240,140,170,90))
pygame.draw.rect(menuScreen,white,pygame.Rect(450,200,140,70))
pygame.draw.rect(menuScreen,white,pygame.Rect(650,140,200,90))
textFont = pygame.font.SysFont('Comic Sans MS', 25)
offlineScreen = textFont.render('Play Offline', False, (0,0,0))
computerScreen = textFont.render('Play Vs Computer', False, (0,0,0))
playScreen = textFont.render('Start', False, (0,0,0))
menuScreen.blit(offlineScreen,(260,170))
menuScreen.blit(playScreen,(485,220))
menuScreen.blit(computerScreen,(651,160))
pygame.display.flip()
offlineBool = False
Functioncall = False
while gameRun == False:
    for event in pygame.event.get():
                if event.type == pygame.QUIT:
                        gameRun = True
                if event.type == pygame.MOUSEBUTTONDOWN:
                    mPosition = pygame.mouse.get_pos()
                    if mPosition[0] > 240 and mPosition[0] < 410:
                        if mPosition[1] > 140 and mPosition[1] < 210:
                            pygame.draw.rect(menuScreen,green,pygame.Rect(240,140,170,90))
                            textFont = pygame.font.SysFont('Comic Sans MS', 25)
                            offlineScreen = textFont.render('Play Offline', False, (0,0,0))
                            menuScreen.blit(offlineScreen,(260,170))
                            pygame.display.flip()
                            offlineBool = True
                    if mPosition[0] > 450 and mPosition[0] < 590:
                        if mPosition[1] > 200 and mPosition[1] < 270:
                            if offlineBool == True:
                                Functioncall = True
                                gameRun = True
pygame.quit()
if Functioncall == True:
    mainFunction()






