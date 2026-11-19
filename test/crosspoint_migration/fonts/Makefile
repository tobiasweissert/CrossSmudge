.PHONY: test sanitize

test:
	python3 -m unittest discover -s . -p 'test_*.py' -v

sanitize:
	FONT_SANITIZE=1 python3 -m unittest discover -s . -p 'test_*.py' -v
