#!/usr/bin/env python3
"""Author a separate five-second vector idle study using the supplied parameters."""
import json
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'assets/lottie/idle_trial_v1'

def value(k,ix=1): return {'a':0,'k':k,'ix':ix}
def keys(points,ix=1,linear=False):
    return {'a':1,'ix':ix,'k':[{'t':t,'s':v,'i':{'x':[1 if linear else .58],'y':[1]},'o':{'x':[0 if linear else .42],'y':[0]}} for t,v in points]}
def transform(p=(0,0),anchor=(0,0),scale=None,rotation=None):
    return {'o':value(100,11),'r':rotation or value(0,10),'p':value([*p,0],2),
            'a':value([*anchor,0],1),'s':scale or value([100,100,100],6)}
def group(items,name):
    tr={'ty':'tr','nm':'Transform','p':value([0,0],2),'a':value([0,0],1),
        's':value([100,100],3),'r':value(0,6),'o':value(100,7),'sk':value(0,4),'sa':value(0,5)}
    for ix,item in enumerate(items,1): item['ix']=ix; item.setdefault('hd',False)
    return {'ty':'gr','nm':name,'ix':1,'np':len(items)+1,'cix':2,'hd':False,
            'mn':'ADBE Vector Group','bm':0,'it':items+[tr]}
def fill(hexcolor):
    rgb=[int(hexcolor[i:i+2],16)/255 for i in (0,2,4)]
    return {'ty':'fl','nm':'Fill','c':value(rgb+[1]),'o':value(100),'r':1,'bm':0}
def ellipse(x,y,w,h,color,name):
    return group([{'ty':'el','nm':name,'p':value([x,y]),'s':value([w,h]),'d':1},fill(color)],name)
def path(start,segments,color,name,stroke=0,closed=True):
    vertices=[list(start)]; incoming=[[0,0]]; outgoing=[]
    for c1,c2,end in segments:
        outgoing.append([c1[0]-vertices[-1][0],c1[1]-vertices[-1][1]])
        vertices.append(list(end)); incoming.append([c2[0]-end[0],c2[1]-end[1]])
    if closed and vertices[-1]==vertices[0]:
        incoming[0]=incoming.pop(); vertices.pop()
    else: outgoing.append([0,0])
    shape={'ty':'sh','nm':name,'ks':value({'v':vertices,'i':incoming,'o':outgoing,'c':closed})}
    paint=fill(color) if not stroke else {'ty':'st','nm':'Stroke','c':fill(color)['c'],'o':value(100),
            'w':value(stroke),'lc':2,'lj':2,'ml':4,'bm':0}
    return group([shape,paint],name)
def layer(index,name,shapes,p=(0,0),parent=100,scale=None,rotation=None):
    d={'ddd':0,'ind':index,'ty':4,'nm':name,'sr':1,'ao':0,'bm':0,'ip':0,'op':150,'st':0,
       'ks':transform(p,scale=scale,rotation=rotation),'shapes':shapes}
    if parent is not None:d['parent']=parent
    return d

def main():
    OUT.mkdir(parents=True,exist_ok=True)
    breath=keys([(0,[100,100,100]),(9,[101.25,101.25,100]),(18,[100,100,100]),
        (33,[102.5,102.5,100]),(48,[100,100,100]),(63,[102.5,102.5,100]),
        (78,[100,100,100]),(93,[102.5,102.5,100]),(108,[100,100,100]),
        (123,[101.25,101.25,100]),(138,[100,100,100]),(150,[100,100,100])],6)
    blink=keys([(0,[100,100,100]),(45,[100,100,100]),(47,[100,10,100]),
                (49,[100,100,100]),(150,[100,100,100])],6,True)
    wing_rotation=keys([(0,[0]),(84,[0]),(99,[12]),(114,[0]),(150,[0])],10)
    eye=[]
    for side,x,y in [('left',190,179),('right',289,184)]:
        eye.append(layer(len(eye)+1,'eye_'+side,[ellipse(4,-7,10,10,'FFFFFF','reflection'),
             ellipse(-6,8,6,6,'63452B','warm_reflection'),ellipse(0,0,34,40,'281A10','eye')],
             (x,y),scale=blink))
    cheeks=[layer(3,'cheeks',[ellipse(161,214,39,31,'EF9B89','left_cheek'),ellipse(316,220,40,32,'EF9B89','right_cheek')])]
    beak=layer(4,'matte_beak',[
      path((218,229),[((222,230),(236,236),(246,234)),((256,236),(267,227),(270,227)),
           ((263,244),(255,258),(244,257)),((235,258),(225,247),(218,229))],'D57422','lower_bill'),
      path((224,231),[((235,225),(252,225),(263,230)),((257,247),(233,249),(224,231))],'A74417','mouth'),
      ellipse(244,246,21,14,'EC7932','tongue'),
      path((215,224),[((226,216),(235,205),(245,206)),((256,207),(265,216),(274,224)),
           ((274,232),(258,234),(245,229)),((232,235),(212,231),(215,224))],'EB943B','upper_bill')])
    beak['shapes'].reverse()  # ThorVG's first group is foreground, like layers.
    brows=layer(5,'brows',[path((165,145),[((172,135),(180,134),(187,137))],'D7A83E','left_brow',4,False),
         path((283,141),[((292,142),(299,147),(302,152))],'D7A83E','right_brow',4,False)])
    tuft=layer(6,'tuft',[path((216,86),[((201,82),(209,65),(222,67)),((232,68),(238,73),(243,78)),
         ((228,59),(229,39),(243,44)),((262,50),(272,70),(270,91)),((252,93),(234,91),(216,86))],'F4D364','two_soft_tufts')])
    wing_segments=[((-18,-15),(-49,-26),(-55,-13)),((-63,-4),(-55,6),(-46,9)),
                   ((-66,8),(-61,29),(-45,29)),((-57,41),(-39,51),(-23,40)),
                   ((-8,38),(2,23),(5,5)),((6,0),(8,-4),(5,-6))]
    leftwing=path((5,-6),wing_segments,'F2CE56','round_left_wing')
    rightwing=path((-5,-6),[tuple((-p[0],p[1]) for p in segment) for segment in wing_segments],'F2CE56','round_right_wing')
    wings=[layer(7,'left_wing',[leftwing],(124,284),rotation=wing_rotation),
           layer(8,'right_wing',[rightwing],(356,284))]
    body=layer(9,'body',[path((241,83),[((300,81),(345,126),(357,189)),
          ((361,218),(383,221),(377,250)),((376,273),(369,281),(360,294)),
          ((369,349),(355,394),(321,405)),((274,426),(204,426),(160,407)),
          ((124,394),(111,353),(121,301)),((101,275),(96,242),(108,221)),
          ((117,155),(163,85),(241,83))],'F3D565','soft_body')])
    foot_segments=[((-9,9),(-14,13),(-25,18)),((-37,26),(-25,36),(-12,28)),
      ((-12,43),(4,42),(11,28)),((22,40),(43,33),(33,23)),((25,17),(13,13),(10,0)),
      ((4,0),(-2,0),(-8,0))]
    feet=[layer(11,'feet',[path((-8,0),foot_segments,'E78B35','left_foot')],(180,414),None),
          layer(12,'feet',[path((-8,0),foot_segments,'E78B35','right_foot')],(299,414),None)]
    root={'ddd':0,'ind':100,'ty':3,'nm':'shared_breath_root','sr':1,'ao':0,'bm':0,'ip':0,'op':150,'st':0,
          'ks':transform((240,416),(240,416),breath)}
    background=layer(200,'cream_background',[group([{'ty':'rc','nm':'background','p':value([240,240]),
        's':value([480,480]),'r':value(0)},fill('F8F2E9')],'background')],parent=None)
    data={'v':'5.5.7','fr':30,'ip':0,'op':150,'w':480,'h':480,'nm':'chick_idle_parameter_trial_v1',
      'ddd':0,'assets':[],'meta':{'g':'Codex vector idle study','a':'','k':'','d':'','tc':''},
      'layers':eye+cheeks+[beak,brows,tuft]+wings+[body]+feet+[root,background]}
    (OUT/'idle.json').write_text(json.dumps(data,ensure_ascii=False,separators=(',',':')))
    print(OUT/'idle.json')

if __name__=='__main__':main()
