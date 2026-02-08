# 빠른 시작

5분 만에 첫 Gyeol 스토리를 작성하고 실행해 보세요.

## 1단계: 스크립트 작성

`hello.gyeol` 파일을 생성합니다:

```
label start:
    hero "Hey there! I'm Gyeol."
    hero "Want to go on an adventure?"
    menu:
        "Sure, let's go!" -> accept
        "No thanks." -> decline

label accept:
    hero "Great! Let's head out together!"

label decline:
    hero "Okay... maybe next time."
```

## 2단계: 컴파일

```bash
GyeolCompiler hello.gyeol -o hello.gyb
```

## 3단계: 플레이

```bash
GyeolTest hello.gyb
```

콘솔에서 인터랙티브 대화가 표시됩니다:

```
[hero] Hey there! I'm Gyeol.
> (Enter 키를 누르세요)
[hero] Want to go on an adventure?
> (Enter 키를 누르세요)
[1] Sure, let's go!
[2] No thanks.
> 1
[hero] Great! Let's head out together!
--- END ---
```

## 변수 추가하기

```
$ courage = 0

label start:
    "You wake up in a dark forest."
    hero "Where am I...?"
    menu:
        "Look around." -> explore
        "Run away." -> flee

label explore:
    hero "I'll be brave and explore."
    $ courage = 1
    jump encounter

label flee:
    hero "Too scary!"
    jump encounter

label encounter:
    "A giant wolf appears!"
    if courage == 1 -> brave else coward

label brave:
    hero "I won't back down!"
    "The wolf, impressed by your courage, steps aside."

label coward:
    hero "Help! Someone save me!"
    "The wolf takes pity and walks away."
```

## 캐릭터 추가하기

게임 엔진에서 사용할 메타데이터와 함께 캐릭터를 정의합니다:

```
character hero:
    displayName: "Hero"
    color: "#4CAF50"

character villain:
    displayName: "Dark Lord"
    color: "#F44336"

label start:
    hero "I've come to stop you!"
    villain "You dare challenge me?"
```

## 엔진 명령어 추가하기

`@`를 사용하여 게임 엔진에 명령을 보냅니다:

```
label start:
    @ bg "forest.png"
    @ bgm "ambient_forest.ogg"
    hero "What a peaceful place."
    @ sfx "footsteps.wav"
    hero "Wait, I hear something..."
```

## 대사에 태그 추가하기

애니메이션, 표정 등을 위한 메타데이터를 대사에 첨부합니다:

```
label start:
    hero "Hello there!" #mood:happy #pose:wave
    hero "This is serious." #mood:angry #pose:arms_crossed
    hero "Goodbye." #mood:sad
```

## 함수 사용하기

```
label start:
    $ greeting = call greet("Hero", 100)
    hero "{greeting}"

label greet(name, hp):
    $ msg = "{if hp > 50}Welcome, strong {name}!{else}Rest well, {name}.{endif}"
    return msg
```

## 다음 단계

- [스크립트 문법](../scripting/syntax.md) - 전체 언어 레퍼런스
- [변수와 표현식](../scripting/variables-and-expressions.md) - 변수에 대한 모든 것
- [Godot 연동](godot-integration.md) - Godot 4.3에서 사용하기
- [디버거](../tools/debugger.md) - 스토리 디버깅하기
