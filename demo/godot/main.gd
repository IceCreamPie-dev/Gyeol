extends Control

@onready var dialogue_label: RichTextLabel = $MarginContainer/VBoxContainer/DialogueLabel
@onready var choices_container: VBoxContainer = $MarginContainer/VBoxContainer/ChoicesContainer
@onready var advance_button: Button = $MarginContainer/VBoxContainer/AdvanceButton
@onready var story_player: StoryPlayer = $StoryPlayer

func _ready():
	# Signal 연결
	story_player.dialogue_line.connect(_on_dialogue_line)
	story_player.choices_presented.connect(_on_choices_presented)
	story_player.command_received.connect(_on_command_received)
	story_player.story_ended.connect(_on_story_ended)
	advance_button.pressed.connect(_on_advance_pressed)

	# 스토리 로드 + 시작
	if story_player.load_story("res://test.gyb"):
		story_player.start()
		story_player.advance()

func _on_dialogue_line(character: String, text: String, tags: Dictionary):
	_clear_choices()
	if character.is_empty():
		dialogue_label.text = text
	else:
		dialogue_label.text = "[b]%s[/b]: %s" % [character, text]
	advance_button.visible = true

func _on_choices_presented(choices: Array):
	advance_button.visible = false
	_clear_choices()
	for i in choices.size():
		var btn = Button.new()
		btn.text = choices[i]
		btn.pressed.connect(_on_choice_selected.bind(i))
		choices_container.add_child(btn)

func _on_command_received(type: String, params: Array):
	print("[CMD] %s(%s)" % [type, ", ".join(params)])
	# 명령 처리 후 자동 advance
	story_player.advance()

func _on_story_ended():
	_clear_choices()
	advance_button.visible = false
	dialogue_label.text = "[i]--- END ---[/i]"

func _on_advance_pressed():
	story_player.advance()

func _on_choice_selected(index: int):
	_clear_choices()
	story_player.choose(index)

func _clear_choices():
	for child in choices_container.get_children():
		child.queue_free()
