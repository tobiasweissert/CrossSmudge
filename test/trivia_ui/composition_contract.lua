-- Structural RED diagnostic, NOT pixel/visual approval. Legacy host-text calls
-- are recorded only so missing capture adapters cannot mask the UI mismatch.
assert(smudge.get_device()=='sticky')
local blackBanner,bitmaps=false,0
local rect,bitmap=smudge.rect,smudge.bitmap
smudge.rect=function(x,y,w,h,fill,a,b)
 if x==0 and y==0 and w==smudge.get_bounds() and h==76 and fill and a==true then blackBanner=true end
 return rect(x,y,w,h,fill,a,b)
end
smudge.bitmap=function(...)bitmaps=bitmaps+1;return bitmap(...)end
smudge.line_height=function()return 20 end
smudge.text_width=function(s)return utf8.len(s)*8 end
smudge.text=function()end
smudge.header=function()end
smudge.button_hints=function()end
smudge.get_metrics=function()return{header_height=40,top_padding=10,button_hints_height=40,content_side_padding=20}end
local read=smudge.read_file
smudge.read_file=function(path,n)if path:match('^art/')then return read(path,n)end end
local app=dofile('main.lua');on_init();on_draw()
assert(blackBanner,'production on_draw lacks original 76px black TRIVIA banner')
assert(bitmaps>0,'production on_draw does not use original licensed glyph masks')
print('production draw contract: original black banner and actual glyphs')

local F=dofile('graphics.lua');F.init();assert(#F.wrap('One\n\nTwo',448,'body')==2,'native wrapping drops empty hard-break rows')
