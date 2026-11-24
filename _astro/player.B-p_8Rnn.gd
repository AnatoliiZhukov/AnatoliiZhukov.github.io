extends CharacterBody2D
class_name Player

## Horizontal speed in pixels per second.
@export var speed: float = 220.0
@export var jump_velocity := -380.0
const COYOTE_TIME = 0.12

var _coyote := 0.0
signal landed(height: float)

func _physics_process(delta: float) -> void:
	if not is_on_floor():
		velocity += get_gravity() * delta
		_coyote -= delta
	else:
		_coyote = COYOTE_TIME
	# Jump while grounded or just after leaving a ledge.
	if Input.is_action_just_pressed("jump") and _coyote > 0.0:
		velocity.y = jump_velocity
	var direction := Input.get_axis("move_left", "move_right")
	velocity.x = direction * speed if direction else move_toward(velocity.x, 0, speed)
	move_and_slide()
	$Sprite2D.flip_h = velocity.x < 0
