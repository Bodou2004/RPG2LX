"""Create RPG2LX-owned MV-format test data and procedural PNG fixtures.

No RPG Maker runtime code or commercial assets are bundled.
Requires Pillow only to regenerate assets; playing the fixture does not need Python.
"""
import json
from pathlib import Path
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1] / "fixtures/playable_project"


def write(path, value):
    target = ROOT / path
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(json.dumps(value, ensure_ascii=False, separators=(",", ":")) + "\n", encoding="utf-8")


def command(code, *parameters):
    return {"code": code, "indent": 0, "parameters": list(parameters)}


def speech(*lines):
    return [command(101, "", 0, 0, 2), *[command(401, line) for line in lines]]


def page(commands, *, trigger=0, priority=1, character="", tile=0, switch=None, through=False):
    return {"conditions": {"switch1Valid": switch is not None, "switch1Id": switch or 1,
                           "switch2Valid": False, "variableValid": False,
                           "selfSwitchValid": False, "itemValid": False, "actorValid": False},
            "image": {"tileId": tile, "characterName": character, "characterIndex": 0,
                      "direction": 2, "pattern": 1}, "trigger": trigger, "priorityType": priority,
            "through": through, "moveType": 0, "walkAnime": True, "stepAnime": False,
            "list": [*commands, command(0)]}


def event(id, name, x, y, pages):
    return {"id": id, "name": name, "x": x, "y": y, "note": "", "pages": pages}


def map_data(name, interior=False):
    w, h = 18, 12
    cells = [2816] * (w*h) + [0] * (w*h*5)
    def put(x, y, tile, layer=1):
        cells[(layer*h+y)*w+x] = tile
    for x in range(w):
        put(x, 0, 2); put(x, h-1, 2)
    for y in range(h):
        put(0, y, 2); put(w-1, y, 2)
    if interior:
        for y in range(1,h-1):
            for x in range(1,w-1): put(x,y,4)
        for x in range(8,14): put(x,4,2)
    else:
        for x in range(2,16): put(x,7,1)
        for y in range(5,8): put(12,y,1)
        for x in range(11,15): put(x,4,2); put(x,3,6,2)
        for y in range(2,5):
            for x in range(2,5): put(x,y,3)
        for x,y in [(7,3),(8,9),(14,8),(9,5),(3,9),(15,2)]: put(x,y,7)
        put(12,5,4)
    return {"displayName": name, "width": w, "height": h, "tilesetId": 1, "scrollType": 0,
            "data": cells, "events": [None], "disableDashing": False, "parallaxName": "",
            "encounterList": [], "encounterStep": 30, "autoplayBgm": False, "autoplayBgs": False}


def assets():
    out = ROOT / "img/tilesets"
    out.mkdir(parents=True, exist_ok=True)
    grass = Image.new("RGBA", (768,576), (50,112,77,255)); d=ImageDraw.Draw(grass)
    for y in range(0,576,6):
        for x in range(0,768,6):
            if (x*17+y*13)%5 == 0: d.line((x,y,x+2,y-2),fill=(68,139,91,255))
    grass.save(out / "Demo_A2.png")
    tiles = Image.new("RGBA", (768,768)); d=ImageDraw.Draw(tiles)
    for tile in range(1,8):
        x=tile*48
        if tile == 1:
            d.rectangle((x,0,x+47,47),fill=(178,149,105))
            for n in range(8): d.point((x+(n*13)%48,(n*17)%48),fill=(211,182,131))
        elif tile == 2:
            d.rectangle((x,0,x+47,47),fill=(52,66,76))
            for yy in (0,16,32):
                for xx in (0,24): d.rectangle((x+xx+1,yy+1,x+xx+22,yy+14),fill=(104,121,131))
        elif tile == 3:
            d.rectangle((x,0,x+47,47),fill=(39,105,145))
            for yy in (9,25,40): d.line((x+4,yy,x+31,yy),fill=(74,158,183),width=2)
        elif tile == 4:
            d.rectangle((x,0,x+47,47),fill=(136,88,62))
            for xx in (0,16,32): d.rectangle((x+xx+1,1,x+xx+14,46),fill=(170,119,74))
        elif tile == 5:
            d.rectangle((x+21,22,x+26,47),fill=(100,68,50))
            d.rectangle((x+5,6,x+42,29),fill=(77,52,39)); d.rectangle((x+7,8,x+40,27),fill=(196,153,91))
            d.line((x+14,15,x+32,15),fill=(99,63,43),width=2)
        elif tile == 6:
            d.rectangle((x,0,x+47,47),fill=(101,46,53))
            for yy in (0,12,24,36): d.line((x,yy,x+47,yy),fill=(159,77,71),width=3)
        elif tile == 7:
            d.line((x+24,26,x+24,42),fill=(86,155,90),width=3)
            for xx,yy in [(20,19),(27,19),(20,26),(27,26)]:
                d.rectangle((x+xx,yy,x+xx+5,yy+5),fill=(247,184,156))
            d.rectangle((x+24,24,x+27,27),fill=(246,218,122))
    tiles.save(out / "Demo_B.png")
    chars=ROOT/'img/characters'; chars.mkdir(parents=True,exist_ok=True)
    for name,coat,hat in [('$Hero',(70,147,188),(227,187,104)),('$Guide',(178,110,125),(218,213,196))]:
        sheet=Image.new('RGBA',(144,192)); d=ImageDraw.Draw(sheet)
        for direction in range(4):
            for step in range(3):
                x=step*48;y=direction*48
                d.ellipse((x+12,y+37,x+36,y+45),fill=(12,26,33,90))
                offset=2 if step==0 else -2 if step==2 else 0
                d.rectangle((x+16,y+31,x+22,y+42+offset),fill=(39,48,66))
                d.rectangle((x+26,y+31,x+32,y+42-offset),fill=(39,48,66))
                d.rectangle((x+14,y+19,x+34,y+34),fill=(27,46,58));d.rectangle((x+16,y+20,x+32,y+32),fill=coat)
                d.rectangle((x+16,y+7,x+32,y+21),fill=(66,48,44)); d.rectangle((x+18,y+9,x+30,y+21),fill=(229,183,145))
                d.rectangle((x+14,y+5,x+34,y+11),fill=hat)
                if direction != 3:
                    d.rectangle((x+(20 if direction!=2 else 28),y+14,x+(21 if direction!=2 else 29),y+16),fill=(39,42,52))
                    if direction==0:d.rectangle((x+28,y+14,x+29,y+16),fill=(39,42,52))
        sheet.save(chars/(name+'.png'))


def main():
    write('data/System.json', {"gameTitle": "RPG2LX - Primer viaje", "startMapId": 1, "startX": 4,
                               "startY": 7, "partyMembers": [1], "hasEncryptedImages": False,
                               "switches": ["","Entrada permitida"], "variables": [""]})
    write('data/Actors.json', [None,{"id":1,"name":"Lucía","characterName":"$Hero","characterIndex":0}])
    write('data/CommonEvents.json',[None])
    write('data/MapInfos.json',[None,{"id":1,"name":"El pueblo"},{"id":2,"name":"La casa"}])
    flags=[0]*8192; flags[0]=16
    flags[2]=15;flags[3]=15;flags[6]=16
    write('data/Tilesets.json',[None,{"id":1,"name":"Original demo tiles",
                                     "tilesetNames":["","Demo_A2","","","","Demo_B","","",""],"flags":flags}])
    village=map_data('El pueblo')
    village['events'] += [
        event(1,'Guía',6,6,[page([*speech('¡Bienvenido a RPG2LX!','Camina, habla y entra en la casa.'),command(121,1,1,0)],character='$Guide')]),
        event(2,'Cartel',4,5,[page(speech('RPG2LX: primer mapa nativo.','Flechas: caminar. A: hablar.','La fuente y los muros bloquean.'),tile=5)]),
        event(3,'Entrada',12,5,[
            page(speech('Primero habla con la guía.'),trigger=1,priority=0,through=True),
            page([command(201,0,2,4,7,2,0)],trigger=1,priority=0,through=True,switch=1)])]
    house=map_data('La casa',True)
    house['events'] += [
        event(1,'Salida',4,9,[page([command(201,0,1,12,6,2,0)],trigger=1,priority=0,through=True)]),
        event(2,'Mensaje',7,7,[page(speech('¡Has cambiado de mapa!','Esto es un evento MV real.','Faltan batalla, audio y plugins.'),character='$Guide')])]
    write('data/Map001.json',village);write('data/Map002.json',house)
    write('package.json',{'name':'rpg2lx-first-journey','window':{'width':816,'height':624}})
    (ROOT/'js').mkdir(exist_ok=True);(ROOT/'js/plugins.js').write_text('var $plugins = [];\n')
    assets()
    print(ROOT)


if __name__=='__main__':main()
