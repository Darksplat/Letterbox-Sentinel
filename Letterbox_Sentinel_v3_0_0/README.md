# Letterbox Sentinel v3.0.0

This release changes mail detection to support multiple independent deliveries before the letterbox is emptied.

## Behaviour

- Monitoring window: Monday-Saturday, 07:30-17:00.
- A genuine beam interruption sets `Mail Waiting` ON, increments the lifetime counter and updates the last-delivery timestamp.
- The IR emitter is switched off for 30 seconds after each genuine detection to prevent duplicate counts from the same item.
- After 30 seconds the emitter automatically turns back on and rearms, even if `Mail Waiting` is still ON.
- Later deliveries are therefore detected and counted without requiring NFC/manual collection first.
- NFC/manual collection clears only the persistent `Mail Waiting` state and records the collector.
- Overnight and Sunday deep-sleep operation remains unchanged.
- Home Assistant MQTT Discovery topic/object IDs and `unique_id` values remain unchanged from v2.2.x.

The public source uses placeholder Wi-Fi, MQTT and OTA credentials. Do not commit real credentials to this public repository.
