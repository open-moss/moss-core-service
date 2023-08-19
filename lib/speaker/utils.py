from os import path
import json
import yaml

def load_config(config_path):
    if not path.exists(config_path):
        raise FileExistsError(f"speaker config is not found: {config_path}")
    with open(config_path, "r", encoding="utf-8") as f:
        data = f.read()
    temp = yaml.load(data, yaml.Loader)
    config = SpeakerConfig(**temp)
    config.model_config = load_model_config(config.model_config_path)
    return config

def load_model_config(model_config_path):
    model_config_path = path.join(path.dirname(__file__), '../../', model_config_path if model_config_path else "models/speaker/moss.json")
    if not path.exists(model_config_path):
        raise FileExistsError(f"speaker model config is not found: {model_config_path}")
    with open(model_config_path, "r", encoding="utf-8") as f:
        data = f.read()
    temp = json.loads(data)
    return SpeakerModelConfig(**temp)

class Base():
    def __init__(self, **kwargs):
        for k, v in kwargs.items():
            if type(v) == dict:
                v = Base(**v)
            self[k] = v

    def keys(self):
        return self.__dict__.keys()

    def items(self):
        return self.__dict__.items()

    def values(self):
        return self.__dict__.values()

    def __len__(self):
        return len(self.__dict__)

    def __getitem__(self, key):
        return getattr(self, key)

    def __setitem__(self, key, value):
        return setattr(self, key, value)

    def __contains__(self, key):
        return key in self.__dict__

    def __repr__(self):
        return self.__dict__.__repr__()

class SpeakerConfig(Base):
    def __init__(self, **kwargs):
        super().__init__(**kwargs)
        self.model_config = None
        if not hasattr(self, "mode"):
            self.mode = "local"
        if not hasattr(self, "sample_rate"):
            self.sample_rate = 16000
        if self.mode == "remote":
            if not hasattr(self, "remote_url"):
                raise ValueError("remote_url is required")
        else:
            if not hasattr(self, "model_path"):
                raise ValueError("model_path is required")
            if not hasattr(self, "model_config_path"):
                raise ValueError("model_config_path is required")

class SpeakerModelConfig(Base):
    def __init__(self, **kwargs):
        super().__init__(**kwargs)

