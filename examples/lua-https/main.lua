-- Real public GET. No fixture response or automatic POST retry.
local message = "Select: fetch example.com. Back: exit."
local request_id
function on_init() smudge.request_update() end
function on_button(button)
  if button == "back" then
    if request_id then smudge.http_cancel(request_id) end
    smudge.exit()
  elseif button == "confirm" and not request_id then
    local id, err = smudge.http_request {
      url = "https://example.com/",
      method = "GET",
      headers = { Accept = "text/html" },
      timeout_ms = 15000
    }
    request_id = id
    message = id and "Fetching over verified TLS..." or ("Not started: " .. err)
    smudge.request_update()
  end
end
function on_http(event)
  request_id = nil
  if event.error then
    message = "Failed: " .. event.error
    if event.error == "time_untrusted" then
      message = message .. ". Sync date/time, then trust UTC in App Settings."
    end
  else
    message = "HTTP " .. event.status .. ": " .. #event.body .. " bytes received from example.com."
  end
  smudge.request_update()
end
function on_draw()
  smudge.clear()
  smudge.header("HTTPS Demo")
  local width = smudge.get_bounds()
  local metrics = smudge.get_metrics()
  local padding = metrics.content_side_padding or 20
  local y = metrics.header_height + metrics.top_padding + 20
  for _, line in ipairs(smudge.wrapped_text(message, width - 2 * padding, "ui12")) do
    smudge.text(padding, y, line, "ui12")
    y = y + smudge.line_height("ui12")
  end
  smudge.button_hints("Back", "Fetch", "", "")
end
