-- support.lua is prepended by the routine runner; data enters real callbacks.
T.open();T.shot('busy');T.update();T.complete('[11,22]')
T.update();T.complete(T.story(11,'Synthetic story one',42));T.update();T.complete(T.story(22,'Synthetic story two',7));T.shot('list')
T.reset();T.update();T.complete('[33]');T.update();T.complete(T.story(33,'Synthetic article',0))
T.tap('story',1);T.update();T.complete('Readable article text.');T.shot('reader')
on_exit();print('PASS native list/reader fixtures through actual provider callbacks')
