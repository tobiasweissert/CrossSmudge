local J=dofile('json.lua')
local M={}
local function decode(s)
  if type(s)~='string' or #s>2048 then return end
  -- RFC3986 encoding: '+' remains '+', and malformed escapes are rejected.
  if s:gsub('%%%x%x',''):find('%%') then return end
  s=s:gsub('%%(%x%x)',function(h) return string.char(tonumber(h,16)) end)
  if #s==0 or s:find('[%z\1-\31\127]') or not utf8.len(s) then return end
  return s
end
function M.question(raw)
  local doc=J.decode(raw)
  if type(doc)~='table' or doc.response_code~=0 or type(doc.results)~='table' or #doc.results~=1 then return end
  local q=doc.results[1]
  if type(q)~='table' or q.type~='multiple' or type(q.incorrect_answers)~='table' or #q.incorrect_answers~=3 then return end
  local question,correct=decode(q.question),decode(q.correct_answer)
  if not question or not correct then return end
  local answers={correct}; local seen={[correct]=true}
  for _,s in ipairs(q.incorrect_answers) do
    local a=decode(s)
    if not a or seen[a] then return end
    answers[#answers+1]=a; seen[a]=true
  end
  return {question=question,correct=correct,answers=answers}
end
return M
